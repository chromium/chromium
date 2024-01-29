import fs from "fs";
import path from "path";
import {
  assert,
  currentPlatform,
  log,
  spawnChecked,
  Platform,
  outputArchitecture,
  getBackendDir,
} from "./common.mjs";
import { readSymbols } from "./symbolication.mjs";

const DEFAULT_BUCKET_NAME = "recordreplay-us-east-2";
const S3Bucket = process.env.RECORDREPLAY_BUCKET || DEFAULT_BUCKET_NAME;
const S3DevBucket = "recordreplay-us-east-2-dev";
const S3Website = "recordreplay-website";

const BUILDKITE_BUILD_ID_ARTIFACT = "build_id";
const BUILDKITE_ARTIFACT_DIRECTORY = path.join(
  process.env.BUILDKITE_BUILD_CHECKOUT_PATH || "./",
  "build_id",
  currentPlatform(),
  outputArchitecture()
);

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

function copyBuildFiles(srcDir, dstDir) {
  function shouldCopyFile(file) {
    const names = [
      // shared
      "icudtl.dat",
      "v8_context_snapshot.bin",
      "vk_swiftshader_icd.json",

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

  for (const file of fs.readdirSync(srcDir)) {
    if (shouldCopyFile(file)) {
      fs.cpSync(path.join(srcDir, file), path.join(dstDir, file));
    }
  }
  fs.cpSync(path.join(srcDir, "locales"), path.join(dstDir, "locales"), {
    recursive: true,
  });
}

function prepareLinuxBinaries(buildId) {
  const buildIdArchive = `${buildId}.tar.xz`;
  const buildArchive = "linux-chromium.tar.xz";

  spawnChecked("rm", ["-rf", "replay-chromium"], { stdio: "inherit" });

  fs.mkdirSync("replay-chromium");

  copyBuildFiles("out/Release", "replay-chromium");

  // Parallel build (requires xz), unlimited cores, w/ reasonable compression.
  spawnChecked(
    "tar",
    ["-c", "-I", "xz -2 -T0", "-f", buildIdArchive, "replay-chromium"],
    {
      stdio: "inherit",
    }
  );
  spawnChecked("cp", [buildIdArchive, buildArchive], { stdio: "inherit" });

  spawnChecked("rm", ["-rf", "replay-chromium"], { stdio: "inherit" });
  return [buildIdArchive];
}

function prepareWindowsBinaries(buildId) {
  const buildArchive = `${buildId}.zip`;
  fs.rmSync("replay-chromium", { force: true, recursive: true });
  fs.mkdirSync("replay-chromium");

  copyBuildFiles(path.join("out", "Release"), "replay-chromium");

  // On windows we need to add a couple OpenSSL DLLs to the archive so that the driver will run.
  // This needs to be fixed, see https://github.com/RecordReplay/backend/issues/2847
  for (const dll of ["libssl-1_1-x64.dll", "libcrypto-1_1-x64.dll"]) {
    fs.copyFileSync(
      path.join(getBackendDir(), "lib", dll),
      path.join("replay-chromium", dll)
    );
  }
  spawnChecked(
    // TODO(dmiller): this is gross, we should control this build dependency
    "C:\\mozilla-build\\bin\\zip.exe",
    ["-r", buildArchive, "replay-chromium"],
    {
      stdio: "inherit",
    }
  );
  fs.rmSync("replay-chromium", { force: true, recursive: true });

  return [buildArchive];
}

function prepareMacOSBinaries(buildId) {
  const buildIdDmgArchive = buildArm ? `${buildId}-arm.dmg` : `${buildId}.dmg`;
  const dmgArchive = buildArm ? "macos-chromium-arm.dmg" : "macos-chromium.dmg";
  const outdir = buildArm ? "out/Release-ARM" : "out/Release";
  fs.rmSync(path.join(outdir, "Replay-Chromium.app"), {
    recursive: true,
    force: true,
  });
  fs.renameSync(
    path.join(outdir, "Chromium.app"),
    path.join(outdir, "Replay-Chromium.app")
  );
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

  spawnChecked(
    "tar",
    ["cfJ", path.join(process.cwd(), buildIdTarArchive), "Replay-Chromium.app"],
    { cwd: outdir }
  );
  fs.renameSync(
    path.join(outdir, "Replay-Chromium.app"),
    path.join(outdir, "Chromium.app")
  );

  fs.cpSync(buildIdDmgArchive, dmgArchive);
  fs.cpSync(buildIdTarArchive, tarArchive);

  return [buildIdDmgArchive, buildIdTarArchive];
}

async function main(options) {
  let buildArchives = [];
  const { buildId, symbolsArchiveFile } = await buildChromiumSymbols(options);

  const platform = currentPlatform();

  switch (platform) {
    case "linux":
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

  let markdownMessage = `# ${platform} (${arch}) links\n\n${markdownDownloadList}\n`;
  if (platform === "linux") {
    // Linux is usually the first. Let's prefix it with relevant Admin App link.
    const buildPattern = buildId.substring(buildId.indexOf("-"));
    const aaCrashTriageLink = `http://admin.replay.io/crash?buildReleaseOptions=DevOnly&builds=${buildPattern}&platforms=macOS&platforms=linux&platforms=windows`;
    const aaCommandCrashTriageLink = `${aaCrashTriageLink}&kind=command`;
    const aaCrashTriageMessage =
      `# Admin App Crash Triage\n` +
      `* [Fatals](${aaCrashTriageLink})\n` +
      `* [Commands](${aaCommandCrashTriageLink})\n`;
    markdownMessage = aaCrashTriageMessage + markdownMessage;
  }

  spawnChecked(
    "buildkite-agent",
    ["annotate", "--append", "--style", "info", markdownMessage],
    {
      stdio: "inherit",
    }
  );

  // Write the build_id artifact.  This is how buildkite agents will know which build
  // to download from S3: by first downloading this file.
  fs.rmSync(BUILDKITE_ARTIFACT_DIRECTORY, { force: true, recursive: true });
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
    let downloadArchive = driverArchive;
    if (driverRevisionIsSet) {
      downloadArchive = `${currentPlatform()}-recordreplay-${driverRevision}.tgz`;
    }
    spawnChecked(
      "curl",
      [
        `https://static.replay.io/downloads/${downloadArchive}`,
        "-o",
        driverArchive,
      ],
      { stdio: "inherit" }
    );

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
