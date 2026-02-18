import fs from "fs";
import path from "path";
import process from "process";
import { spawnSync } from "child_process";

import {
  assert,
  currentPlatform,
  log,
  spawnChecked,
  Platform,
  getBackendDir,
  getArtifactDir,
} from "./common.mjs";
import { readSymbols } from "./symbolication.mjs";

const DEFAULT_BUCKET_NAME = "recordreplay-us-east-2";
const S3Bucket = process.env.RECORDREPLAY_BUCKET || DEFAULT_BUCKET_NAME;
const S3DevBucket = "recordreplay-us-east-2-dev";
const S3Website = "recordreplay-website";

const BUILDKITE_BUILD_ID_ARTIFACT = "build_id";
const BUILDKITE_ARTIFACT_DIRECTORY = getArtifactDir();

// If this env var is set, we then we (also) use this as our cue that
// we're building on a developer's machine, and will run some different
// logic.
const IS_LOCAL_BUILD = !!process.env["LOCAL_DEVELOPER_BUILD_EXTENSION"];

const buildArm = !!process.env.REPLAY_BUILD_ARM;

function uploadToAllBuckets(localPath, s3Path) {
  const buckets = [S3DevBucket];
  // If we're on a local machine, don't include the prod S3 bucket.
  if (!IS_LOCAL_BUILD) {
    buckets.push(S3Bucket);
  }
  for (const bucket of buckets) {
    spawnChecked(
      "aws",
      [
        "s3",
        "cp",
        "--acl",
        "bucket-owner-full-control",
        localPath,
        `s3://${bucket}/${s3Path}`,
      ],
      {
        stdio: "inherit",
      }
    );
  }
}

function copySync(srcDir, dstDir, file) {
  fs.cpSync(path.join(srcDir, file), path.join(dstDir, file), {
    recursive: true,
  });
}

function copyBuildFiles(dstDir) {
  // macOS already has all the build files in the proper place in its app bundles
  assert(currentPlatform() !== Platform.macOS);

  const outDir = path.join("out", "Release");
  function shouldCopyFile(file) {
    const names = [
      // shared
      "icudtl.dat",
      "v8_context_snapshot.bin",
      "vk_swiftshader_icd.json",
      "replay",

      // linux
      "chrome",
      "chrome_crashpad_handler",

      // windows
      "chrome.exe",
      "chrome.dll",
      "chrome_elf.dll",
    ];

    const extensions = [
      // shared
      ".pak",
      ".pak.info",

      // linux
      ".so",
      ".so.1",

      // windows
      ".manifest",
    ];

    if (names.includes(file)) {
      return true;
    }

    if (extensions.some((extension) => file.endsWith(extension))) {
      return true;
    }

    return false;
  }

  for (const file of fs.readdirSync(outDir)) {
    if (shouldCopyFile(file)) {
      copySync(outDir, dstDir, file);
    }
  }
  copySync(outDir, dstDir, "locales");
  copySync(outDir, dstDir, "replay-assets");
}

// every prepare function below does the same steps:
// 1. delete/create the releaseArtifactDir (using mkdir for windows/linux, renaming for macOS)
// 2. (optionally) copy any necessary build files into the releaseArtifactDir.
// 3. create the binary artifact from the releaseArtifactDir (.tar.xz for linux, .zip for windows, .dmg for macOS)
// 4. clean up the releaseArtifactDir.

function prepareLinuxBinaries(buildId) {
  const releaseArtifactDir = "replay-chromium";
  const buildIdArchive = `${buildId}.tar.xz`;

  fs.rmSync(releaseArtifactDir, { force: true, recursive: true });
  fs.mkdirSync(releaseArtifactDir);

  copyBuildFiles(releaseArtifactDir);

  // Parallel build (requires xz), unlimited cores, w/ reasonable compression.
  spawnChecked(
    "tar",
    ["-c", "-I", "xz -2 -T0", "-f", buildIdArchive, releaseArtifactDir],
    {
      stdio: "inherit",
    }
  );

  fs.rmSync(releaseArtifactDir, { force: true, recursive: true });
  return [buildIdArchive];
}

function prepareWindowsBinaries(buildId) {
  const releaseArtifactDir = "replay-chromium";
  const buildIdArchive = `${buildId}.zip`;
  fs.rmSync(releaseArtifactDir, { force: true, recursive: true });
  fs.mkdirSync(releaseArtifactDir);

  copyBuildFiles(releaseArtifactDir);

  // On windows we need to add a couple OpenSSL DLLs to the archive so that the driver will run.
  // This needs to be fixed, see https://github.com/RecordReplay/backend/issues/2847
  for (const dll of ["libssl-1_1-x64.dll", "libcrypto-1_1-x64.dll"]) {
    copySync(path.join(getBackendDir(), "lib"), releaseArtifactDir, dll);
  }
  spawnChecked(
    // TODO(dmiller): this is gross, we should control this build dependency
    "C:\\mozilla-build\\bin\\zip.exe",
    ["-r", buildIdArchive, releaseArtifactDir],
    {
      stdio: "inherit",
    }
  );
  fs.rmSync(releaseArtifactDir, { force: true, recursive: true });

  return [buildIdArchive];
}

function prepareMacOSBinaries(buildId) {
  const buildIdDmgArchive = buildArm ? `${buildId}-arm.dmg` : `${buildId}.dmg`;
  const dmgArchive = buildArm ? "macos-chromium-arm.dmg" : "macos-chromium.dmg";
  const signedDmg = buildArm
    ? `${buildId}-arm-signed.dmg`
    : `${buildId}-signed.dmg`;
  const outdir = buildArm ? "out/Release-ARM" : "out/Release";
  const appPath = path.join(outdir, "Replay-Chromium.app");

  // Clean up.
  fs.rmSync(appPath, {
    recursive: true,
    force: true,
  });
  fs.renameSync(path.join(outdir, "Chromium.app"), appPath);

  // Bundle dmg file.
  spawnChecked(
    "hdiutil",
    [
      "create",
      path.join(process.cwd(), buildIdDmgArchive),
      "-ov",
      "-volname",
      "Replay-Chromium",
      "-fs",
      "HFS+",
      "-srcfolder",
      "Replay-Chromium.app",
    ],
    { cwd: outdir, stdio: "inherit" }
  );

  const buildIdTarArchive = buildArm
    ? `${buildId}-arm.tar.xz`
    : `${buildId}.tar.xz`;
  const tarArchive = buildArm
    ? "macos-chromium-arm.tar.xz"
    : "macos-chromium.tar.xz";

  // Bundle tar ball.
  spawnChecked(
    "tar",
    ["cfJ", path.join(process.cwd(), buildIdTarArchive), "Replay-Chromium.app"],
    { cwd: outdir }
  );

  // Mac Code Signing.
  // TODO: https://linear.app/replay/issue/RUN-3225/remove-code-signing-temporarily
  const shouldCodesign = false; // !!process.env["REPLAY_APPLE_CODESIGN_PATH"];

  if (shouldCodesign) {
    const anyCodesignEnvVarIsNotSet = [
      "REPLAY_APPLE_CODESIGN_PATH",
      "REPLAY_APPLE_CODESIGN_CERT_PATH",
      "REPLAY_APPLE_CODESIGN_CERT_PASS_PATH",
      "REPLAY_APP_STORE_CONNECT_API_KEY_PATH",
    ].some((envVar) => !process.env[envVar]);
    if (anyCodesignEnvVarIsNotSet) {
      console.error("Missing codesign environment variables", {
        REPLAY_APPLE_CODESIGN_PATH:
          process.env["REPLAY_APPLE_CODESIGN_PATH"] || "missing",
        REPLAY_APPLE_CODESIGN_CERT_PATH:
          process.env["REPLAY_APPLE_CODESIGN_CERT_PATH"] || "missing",
        REPLAY_APPLE_CODESIGN_CERT_PASS_PATH:
          process.env["REPLAY_APPLE_CODESIGN_CERT_PASS_PATH"] || "missing",
        REPLAY_APP_STORE_CONNECT_API_KEY_PATH:
          process.env["REPLAY_APP_STORE_CONNECT_API_KEY_PATH"] || "missing",
      });
    }
    const originalWorkingDir =
      process.env["REPLAY_ORIGINAL_WORKING_DIR"] || process.cwd();
    const codesignPath = process.env["REPLAY_APPLE_CODESIGN_PATH"];
    const fullCodesignPath = path.join(originalWorkingDir, codesignPath);
    const p12FilePath = path.join(
      originalWorkingDir,
      process.env["REPLAY_APPLE_CODESIGN_CERT_PATH"]
    );
    const p12PassPath = path.join(
      originalWorkingDir,
      process.env["REPLAY_APPLE_CODESIGN_CERT_PASS_PATH"]
    );
    const appStoreApiKeyPath = path.join(
      originalWorkingDir,
      process.env["REPLAY_APP_STORE_CONNECT_API_KEY_PATH"]
    );
    const pathsToSign = [
      "Contents/MacOS/Chromium",
      "Contents/Frameworks/Chromium Framework.framework/Versions/108.0.5359.0/Helpers/app_mode_loader",
      "Contents/Frameworks/Chromium Framework.framework/Versions/108.0.5359.0/Helpers/Chromium Helper (Alerts).app/Contents/MacOS/Chromium Helper (Alerts)",
      "Contents/Frameworks/Chromium Framework.framework/Versions/108.0.5359.0/Helpers/Chromium Helper (GPU).app/Contents/MacOS/Chromium Helper (GPU)",
      "Contents/Frameworks/Chromium Framework.framework/Versions/108.0.5359.0/Helpers/Chromium Helper (Plugin).app/Contents/MacOS/Chromium Helper (Plugin)",
      "Contents/Frameworks/Chromium Framework.framework/Versions/108.0.5359.0/Helpers/Chromium Helper (Renderer).app/Contents/MacOS/Chromium Helper (Renderer)",
      "Contents/Frameworks/Chromium Framework.framework/Versions/108.0.5359.0/Helpers/Chromium Helper.app/Contents/MacOS/Chromium Helper",
      "Contents/Frameworks/Chromium Framework.framework/Versions/108.0.5359.0/Helpers/chrome_crashpad_handler",
    ];

    const codeSignatureValues = pathsToSign.map((path) => `${path}:runtime`);
    const codeSignatureFlags = codeSignatureValues.flatMap((value) => [
      "--code-signature-flags",
      value,
    ]);

    spawnChecked(
      fullCodesignPath,
      [
        "sign",
        "--p12-file",
        p12FilePath,
        "--p12-password-file",
        p12PassPath,
        ...codeSignatureFlags,
        appPath,
      ],
      { stdio: "inherit" }
    );
    spawnChecked(
      "hdiutil",
      [
        "create",
        path.join(process.cwd(), signedDmg),
        "-ov",
        "-volname",
        "Replay-Chromium",
        "-fs",
        "HFS+",
        "-srcfolder",
        "Replay-Chromium.app",
      ],
      { cwd: outdir, stdio: "inherit" }
    );
    spawnChecked(
      fullCodesignPath,
      [
        "sign",
        "--p12-file",
        p12FilePath,
        "--p12-password-file",
        p12PassPath,
        signedDmg,
      ],
      { stdio: "inherit" }
    );
    spawnChecked(
      fullCodesignPath,
      [
        "notary-submit",
        "--api-key-file",
        appStoreApiKeyPath,
        "--staple",
        signedDmg,
      ],
      { stdio: "inherit" }
    );
  } else {
    log("Skipping signing/notarization of dmg");
  }

  // Clean up.
  fs.renameSync(appPath, path.join(outdir, "Chromium.app"));

  // Move things into place.
  fs.cpSync(buildIdDmgArchive, dmgArchive);
  fs.cpSync(buildIdTarArchive, tarArchive);

  const buildArtifacts = [buildIdDmgArchive, buildIdTarArchive];
  if (shouldCodesign) {
    buildArtifacts.push(signedDmg);
  }

  return buildArtifacts;
}

async function main(options) {
  let buildArchives = [];
  const { buildId, symbolsArchiveFile } = await buildChromiumSymbols(options);

  const platform = currentPlatform();

  switch (platform) {
    case Platform.linux:
      buildArchives = prepareLinuxBinaries(buildId);
      break;

    case Platform.macOS:
      buildArchives = prepareMacOSBinaries(buildId);
      break;

    case Platform.windows:
      buildArchives = prepareWindowsBinaries(buildId);
      break;

    default:
      throw new Error("Not yet implemented");
  }

  log(`Pushing Artifacts to S3`);

  const downloadUris = uploadArchives(buildArchives);

  uploadToAllBuckets(symbolsArchiveFile, `symbols/${symbolsArchiveFile}`);
  downloadUris.push(`s3://${S3Bucket}/symbols/${symbolsArchiveFile}`);

  for (const buildArchive of buildArchives) {
    log(`BuildUploaded https://static.replay.io/downloads/${buildArchive}`);
  }

  // Perform all buildkite-specific stuff
  if (process.env["BUILDKITE"]) {
    const pakSizesFile = recordPAKSizes(options);
    const entriesFile = recordGClientEntries(options);

    buildkiteStuff(
      downloadUris,
      platform,
      buildId,
      buildArm ? "arm64" : "x86_64",
      symbolsArchiveFile,
      pakSizesFile,
      entriesFile
    );

    fs.unlinkSync(pakSizesFile);
    fs.unlinkSync(entriesFile);
  }
  fs.unlinkSync(symbolsArchiveFile);
}

function buildIdToBuildIdSubset(buildId) {
  const buildIdParts = buildId.split("-");
  return `${buildIdParts[2]}-${buildIdParts[3]}-${buildIdParts[4]}`;
}

function buildkiteStuff(
  downloadUris,
  platform,
  buildId,
  arch,
  symbolsArchiveFile,
  pakSizesFile,
  entriesFile
) {
  const markdownDownloadList = downloadUris
    .map((uri) =>
      uri.replace("s3://recordreplay-website", "https://static.replay.io")
    )
    .map((uri) => `* [${path.basename(uri)}](${uri})`)
    .join("\n");

  let buildsMessage = `# ${platform} (${arch}) links\n\n${markdownDownloadList}\n`;
  if (platform === "linux") {
    // Linux is usually the first. Let's prefix it with relevant Admin App link.
    const buildPattern = buildId.substring(buildId.indexOf("-"));
    const aaCrashTriageLink = `http://admin.replay.io/crash?buildReleaseOptions=DevOnly&builds=${buildPattern}&platforms=macOS&platforms=linux&platforms=windows`;
    const aaCommandCrashTriageLink = `${aaCrashTriageLink}&kind=command`;
    const aaCrashTriageMessage =
      `# Admin App Crash Triage\n` +
      `* [Fatals](${aaCrashTriageLink})\n` +
      `* [Commands](${aaCommandCrashTriageLink})\n`;
    buildsMessage = aaCrashTriageMessage + buildsMessage;
  }

  spawnChecked(
    "buildkite-agent",
    ["annotate", "--append", "--style", "info", buildsMessage],
    {
      stdio: "inherit",
    }
  );

  const buildIdSubset = buildIdToBuildIdSubset(buildId);
  const buildIdMessage = `# Build ID subset \n\n_Copy this when you're releasing this build and the release pipeline asks you for the build ID subset_\n\n\`${buildIdSubset}\`\n`;
  spawnChecked(
    "buildkite-agent",
    [
      "annotate",
      "--context",
      "build-id-subset",
      "--style",
      "info",
      buildIdMessage,
    ],
    {
      stdio: "inherit",
    }
  );

  // Write the build_id artifact.  This is how buildkite agents will know which build
  // to download from S3: by first downloading this file.
  fs.mkdirSync(BUILDKITE_ARTIFACT_DIRECTORY, { recursive: true });
  fs.writeFileSync(
    path.join(BUILDKITE_ARTIFACT_DIRECTORY, BUILDKITE_BUILD_ID_ARTIFACT),
    buildId
  );
  fs.cpSync(
    symbolsArchiveFile,
    path.join(BUILDKITE_ARTIFACT_DIRECTORY, symbolsArchiveFile)
  );
  fs.cpSync(pakSizesFile, path.join(BUILDKITE_ARTIFACT_DIRECTORY, "pak-sizes"));
  fs.cpSync(entriesFile, path.join(BUILDKITE_ARTIFACT_DIRECTORY, "entries"));

  log(
    `Wrote build_id to ${BUILDKITE_ARTIFACT_DIRECTORY}/${BUILDKITE_BUILD_ID_ARTIFACT}`
  );
}

function uploadArchives(buildArchives) {
  const downloadUris = [];
  for (const buildArchive of buildArchives) {
    // Push build to S3.
    uploadToAllBuckets(buildArchive, `builds/${buildArchive}`);
    fs.unlinkSync(buildArchive);

    // Don't copy archives if we're on a local developer's machine.
    if (!IS_LOCAL_BUILD) {
      // Copy build to downloads folder.
      const s3WebsiteUri = `s3://${S3Website}/downloads/${buildArchive}`;
      spawnChecked(
        "aws",
        [
          "s3",
          "cp",
          "--cache-control",
          "max-age=3600",
          `s3://${S3Bucket}/builds/${buildArchive}`,
          s3WebsiteUri,
        ],
        { stdio: "inherit" }
      );
      downloadUris.push(s3WebsiteUri);
    }
  }
  return downloadUris;
}

async function buildChromiumSymbols(options) {
  log(`ChromiumSymbols Start`);

  const buildIdContents = fs.readFileSync(
    `base/record_replay_driver.cc`,
    "utf8"
  );
  const match = /gBuildId\[\] = "(.*?)"/.exec(buildIdContents);

  assert(match);
  const buildId = match[1];

  // Only do build id checking if we're in CI.  On local builds, we randomly generate
  // the runtime revision to ensure s3 freshness.
  if (!IS_LOCAL_BUILD) {
    const expectedBuildId = computeBuildId(
      "chromium",
      readShortRevision(),
      options.driverRevision,
      options.buildIdExtension
    );

    assert(
      buildId == expectedBuildId,
      `Build ID mismatch: expected ${expectedBuildId} got ${buildId}`
    );
  }

  const libraries = [];
  const pdbs = [];
  switch (currentPlatform()) {
    case Platform.macOS:
      libraries.push(
        `Chromium Framework.framework/Versions/Current/Chromium Framework`
      );
      break;
    case Platform.linux:
      libraries.push("chrome");
      break;
    case Platform.windows:
      libraries.push("chrome.dll", "chrome.exe");
      pdbs.push("chrome.dll.pdb", "chrome.exe.pdb");
      break;
    default:
      throw new Error("NYI");
  }

  const releaseDir = options.useARM ? "Release-ARM" : "Release";
  const archiveFile = await buildSymbolsArchive(
    `${buildId}`,
    path.join("out", releaseDir),
    libraries,
    options.useARM,
    pdbs
  );

  log(`ChromiumSymbols Done (${archiveFile})})`);
  return { buildId, symbolsArchiveFile: archiveFile };
}

function readShortRevision(branch = "HEAD") {
  return spawnChecked("git", ["rev-parse", "--short=12", branch])
    .stdout.toString()
    .trim();
}

function driverExtension() {
  return currentPlatform() == "windows" ? "dll" : "so";
}

function buildDateStringToDate(buildDate) {
  const y = buildDate.substring(0, 4);
  const m = buildDate.substring(4, 6);
  const d = buildDate.substring(6);

  return new Date(`${y}-${m}-${d}`);
}

function getLinkerRevisionDate(revision = "HEAD", spawnOptions) {
  const dateString = spawnChecked(
    "git",
    ["show", revision, "--pretty=%cd", "--date=iso-strict", "--no-patch"],
    spawnOptions
  )
    .stdout.toString()
    .trim();

  // convert to UTC -> then get the date only
  // explanations: https://github.com/replayio/backend/pull/7115#issue-1587869475
  return new Date(dateString).toISOString().substring(0, 10).replace(/-/g, "");
}

function computeBuildId(
  runtimeName,
  runtimeRevision,
  driverRevision,
  buildIdExtension
) {
  // Download the archive for this driver revision, using the latest version
  // if no revision was specified.
  let driverJSONStr = "";
  const driverJSONFile = `${currentPlatform()}-recordreplay.json`;

  const driverRevisionIsSet = !!driverRevision;

  if (process.env["REPLAY_LOCAL_DRIVER_DIR"]) {
    const driverJSONFileFull = path.resolve(
      process.env["REPLAY_LOCAL_DRIVER_DIR"],
      driverJSONFile
    );
    driverJSONStr = fs.readFileSync(driverJSONFileFull, "utf8");
  } else {
    const driverFile = `${currentPlatform()}-recordreplay.${driverExtension()}`;
    const driverArchive = `${currentPlatform()}-recordreplay.tgz`;

    if (driverRevisionIsSet) {
      const downloadArchive = `${currentPlatform()}-recordreplay-${driverRevision
        .trim()
        .substring(0, 12)}.tgz`;
      const downloadUrl = `https://static.replay.io/downloads/${downloadArchive}`;
      spawnChecked(
        "curl",
        ["-f", downloadUrl, "-o", driverArchive],
        { stdio: "inherit" }
      );
    } else {
      const downloadUrl = `https://static.replay.io/downloads/${driverArchive}`;
      log(`Downloading latest driver: ${downloadUrl}`);
      spawnChecked(
        "curl",
        ["-f", downloadUrl, "-o", driverArchive],
        { stdio: "inherit" }
      );
    }

    spawnChecked("tar", ["xf", driverArchive]);

    driverJSONStr = fs.readFileSync(driverJSONFile, "utf8");
    fs.unlinkSync(driverArchive);
    fs.unlinkSync(driverFile);
    fs.unlinkSync(driverJSONFile);
  }
  const { revision: archiveDriverRevision, date: driverDate } =
    JSON.parse(driverJSONStr);

  if (driverRevisionIsSet) {
    assert(driverRevision == archiveDriverRevision);
  } else {
    driverRevision = archiveDriverRevision;
  }

  // Get the date when the runtime revision was committed.
  const runtimeDate = getLinkerRevisionDate(runtimeRevision);

  // Use the later of the two dates in the build ID.
  const date =
    buildDateStringToDate(runtimeDate) >= buildDateStringToDate(driverDate)
      ? runtimeDate
      : driverDate;

  return `${currentPlatform()}-${runtimeName}-${date}-${runtimeRevision}-${driverRevision}${buildIdExtension}`;
}

function recordPAKSizes(options) {
  // Record the size of each pak file we care about.
  const pakFiles = ["resources.pak"];

  const releaseDir = options.useARM ? "Release-ARM" : "Release";
  // the containing directory (a subdir of releaseDir) of these files varies by Platform
  let pakDir;
  switch (currentPlatform()) {
    case Platform.macOS:
      pakDir = path.join(
        "out",
        releaseDir,
        "Chromium Framework.framework/Versions/Current/Resources"
      );
      break;
    case Platform.linux:
      pakDir = path.join("out", releaseDir);
      break;
    case Platform.windows:
      throw new Error("No Clue Yet");
      break;
    default:
      throw new Error("NYI");
  }

  let pakSizes = "";
  for (const pakFile of pakFiles) {
    const file = path.join(pakDir, pakFile);
    const size = fs.statSync(file).size;
    pakSizes += `${pakFile} ${size}\n`;
  }

  const archSuffix = options.useArm ? "-arm" : "";
  const pakSizesFile = `pak-sizes${archSuffix}`;
  fs.writeFileSync(pakSizesFile, pakSizes);
  return pakSizesFile;
}

function recordGClientEntries(options) {
  // TODO(toshok) this is probably broken on windows.  need to look what the filename is there.
  let gclient_entries_contents = fs.readFileSync("../.gclient_entries", "utf8");

  let third_party_entries = {};
  gclient_entries_contents.split("\n").forEach((line) => {
    if (!line.includes("src/third_party/")) {
      // we skip all non-third_party lines
      return;
    }
    const match = line.match(/\'src\/third_party\/([^\']+)\': \'([^\']+)\'/);
    if (!match) {
      throw new Error(`Unexpected line format: ${line}`);
    }
    third_party_entries[match[1]] = match[2];
  });

  const keys = Object.keys(third_party_entries);
  keys.sort();
  const output = keys.map((k) => `${k} ${third_party_entries[k]}`).join("\n");

  const archSuffix = options.useArm ? "-arm" : "";
  const entriesFile = `entries${archSuffix}`;
  fs.writeFileSync(entriesFile, output);

  return entriesFile;
}

async function buildSymbolsArchive(
  buildId,
  objectDirectory,
  libraries,
  useArm,
  pdbs = []
) {
  const json = {};
  for (let i = 0; i < libraries.length; i++) {
    const lib = libraries[i];
    const name = path.basename(lib);
    const file = path.join(objectDirectory, lib);
    assert(fs.existsSync(file), `Missing binary for symbols archive ${file}`);
    let pdbFile;
    if (pdbs[i]) {
      pdbFile = path.join(objectDirectory, pdbs[i]);
      assert(
        fs.existsSync(pdbFile),
        `Missing PDB for symbols archive ${pdbFile}`
      );
    }
    const symbols = await readSymbols(file, pdbFile);
    json[name] = symbols;
  }

  const archSuffix = useArm ? "-arm" : "";

  const jsonFile = `${buildId}${archSuffix}.symbols.json`;
  const archiveFile = `${buildId}${archSuffix}.symbols.tgz`;
  fs.writeFileSync(jsonFile, JSON.stringify(json));
  spawnChecked("tar", ["-czf", archiveFile, jsonFile]);
  fs.unlinkSync(jsonFile);

  return archiveFile;
}

const buildIdExtension =
  process.env["BUILDKITE_BRANCH"] !==
  process.env["BUILDKITE_PIPELINE_DEFAULT_BRANCH"]
    ? "-dev"
    : process.env["LOCAL_DEVELOPER_BUILD_EXTENSION"] || "";
const useARM = !!process.env.REPLAY_BUILD_ARM;
main({ buildIdExtension, driverRevision: process.env.DRIVER_REVISION, useARM });
