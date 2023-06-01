import fs from "fs";
import path from "path";
import {
  assert,
  currentPlatform,
  log,
  spawnChecked,
  Platform,
  outputArchitecture,
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
  const buildArchive = `${buildId}.tar.xz`;

  spawnChecked("sudo", ["rm", "-rf", "replay-chromium"], { stdio: "inherit" });

  fs.mkdirSync("replay-chromium");

  copyBuildFiles("out/Release", "replay-chromium");

  // Parallel build (requires xz), unlimited cores, w/ reasonable compression.
  spawnChecked("tar", ["-c", "-I", "xz -2 -T0", "-f", buildArchive, "replay-chromium"], {
    stdio: "inherit",
  });

  spawnChecked("sudo", ["rm", "-rf", "replay-chromium"], { stdio: "inherit" });
  return [buildArchive];
}

function prepareWindowsBinaries(buildId) {
  const buildArchive = `${buildId}.zip`;
  fs.rmSync("replay-chromium", { force: true, recursive: true });
  fs.mkdirSync("replay-chromium");
  spawnChecked(
    "node",
    [copyBuild, "out\\Release", path.join(process.cwd(), "replay-chromium")],
    { cwd: chromium, stdio: "inherit" }
  );
  // On windows we need to add a couple OpenSSL DLLs to the archive so that the driver will run.
  // This needs to be fixed, see https://github.com/RecordReplay/backend/issues/2847
  for (const dll of ["libssl-1_1-x64.dll", "libcrypto-1_1-x64.dll"]) {
    fs.copyFileSync(
      path.join(backend, "lib", dll),
      path.join("replay-chromium", dll)
    );
  }
  spawnChecked(
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
  const dmgArchive = `${buildId}.dmg`;
  const outdir = path.join("out", "Release");
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
      path.join(process.cwd(), dmgArchive),
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
  const tarArchive = `${buildId}.tar.xz`;

  spawnChecked(
    "tar",
    ["cfJ", path.join(process.cwd(), tarArchive), "Replay-Chromium.app"],
    { cwd: outdir }
  );
  fs.renameSync(
    path.join(outdir, "Replay-Chromium.app"),
    path.join(outdir, "Chromium.app")
  );

  return [dmgArchive, tarArchive];
}

async function main(options) {
  let buildArchives = [];
  const buildId = await buildChromiumSymbols(options);

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

  const symbolsFile = `${buildId}.symbols.tgz`;
  uploadToAllBuckets(symbolsFile, `symbols/${symbolsFile}`);
  fs.unlinkSync(symbolsFile);

  for (const buildArchive of buildArchives) {
    log(`BuildUploaded https://static.replay.io/downloads/${buildArchive}`);
  }

  // Perform all buildkite-specific stuff
  if (process.env["BUILDKITE"]) {
    buildkiteStuff(downloadUris, platform, buildId);
  }
}

function buildkiteStuff(downloadUris, platform, buildId) {
  const markdownDownloadList = downloadUris
    .map((uri) => uri.replace("s3://recordreplay-website", "https://static.replay.io")
    )
    .map((uri) => `* [${path.basename(uri)}](${uri})`)
    .join("\n");

  const markdownMessage = `# ${platform} links\n\n${markdownDownloadList}\n`;

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

  await buildSymbolsArchive(
    `${buildId}`,
    path.join("out", "Release"),
    libraries,
    pdbs
  );

  log(`ChromiumSymbols Done`);
  return `${buildId}`;
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

function computeBuildId(runtimeName, runtimeRevision, driverRevision, buildIdExtension) {
  // Download the archive for this driver revision, using the latest version
  // if no revision was specified.
  let driverJSONStr = "";
  const driverJSONFile = `${currentPlatform()}-recordreplay.json`;

  if (process.env["REPLAY_LOCAL_DRIVER_DIR"]) {
    const driverJSONFileFull = path.resolve(process.env["REPLAY_LOCAL_DRIVER_DIR"], driverJSONFile);
    driverJSONStr = fs.readFileSync(driverJSONFileFull, "utf8");
  } else {
    const driverFile = `${currentPlatform()}-recordreplay.${driverExtension()}`;

    const driverArchive = `${currentPlatform()}-recordreplay.tgz`;
    let downloadArchive = driverArchive;
    if (driverRevision) {
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
  
    driverJSONStr = fs.readFileSync(driverJSONFile, "utf8")
    fs.unlinkSync(driverArchive);
    fs.unlinkSync(driverFile);
    fs.unlinkSync(driverJSONFile);  
  }
  const { revision: archiveDriverRevision, date: driverDate } = JSON.parse(driverJSONStr);

  if (driverRevision) {
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

async function buildSymbolsArchive(
  buildId,
  objectDirectory,
  libraries,
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

  const jsonFile = `${buildId}.symbols.json`;
  const archiveFile = `${buildId}.symbols.tgz`;
  fs.writeFileSync(jsonFile, JSON.stringify(json));
  spawnChecked("tar", ["-czf", archiveFile, jsonFile]);
  fs.unlinkSync(jsonFile);
}

const buildIdExtension = process.env["BUILDKITE"] ? "-buildkite" : (process.env["LOCAL_DEVELOPER_BUILD_EXTENSION"] || "");
main({buildIdExtension, driverRevision: process.env.DRIVER_REVISION});
