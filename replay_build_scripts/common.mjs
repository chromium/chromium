import { spawnSync } from "child_process";
import * as fs from "fs";
import * as path from "path";
import { fileURLToPath } from "url";
import { dirname } from "path";

// NOTE(dmiller): see https://stackoverflow.com/a/62892482 for explanation
const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

export function spawnChecked(cmd, args, options) {
  const prettyCmd = [cmd].concat(args).join(" ");
  log("$" + prettyCmd);

  const rv = spawnSync(cmd, args, options);

  if (rv.status != 0 || rv.error) {
    console.group(`Spawn FAILED (${rv.error || ""}) - All output:\n`);
    const stdout = rv.stdout ? rv.stdout.toString() : "";
    const stderr = rv.stderr ? rv.stderr.toString() : "";
    const allOutput = `${stdout} ${stderr}`.trim();
    if (allOutput) {
      console.error(allOutput);
    }
    console.groupEnd();
    throw new Error(`Spawned process failed with exit code ${rv.status}`);
  }

  return rv;
}

export const Platform = {
  macOS: "macOS",
  linux: "linux",
  windows: "windows",
};

// Get the ID for the current platform we are running on.
export function currentPlatform() {
  switch (process.platform) {
    case "darwin":
      return Platform.macOS;
    case "linux":
      return Platform.linux;
    case "win32":
      return Platform.windows;
    default:
      throw new Error(`Platform ${process.platform} not supported`);
  }
}

export function outputArchitecture() {
  switch (currentPlatform()) {
    case Platform.macOS:
      if (process.env.REPLAY_BUILD_ARM) {
        return "arm64";
      }
      return "x86_64";
    case Platform.linux:
      return "x86_64";
    case Platform.windows:
      return "x86_64";
  }
}

export function assert(v, why = "") {
  if (!v) {
    const error = new Error(`Assertion Failed: ${why}`);
    error.name = "AssertionFailure";
    throw error;
  }
}

export function log(s) {
  console.log(s);
}

export function toNumber(str) {
  const rv = +str;
  if (Number.isNaN(rv) && str != "(nil)") {
    throw new Error(`toNumber failed: "${str}"`);
  }
  return rv;
}

export function syncRepo(dir, treeish) {
  log(`Syncing ${dir} to ${treeish}`);
  try {
    spawnChecked("git", ["fetch", "--all"], { cwd: dir, stdio: "inherit" });
  } catch (e) {
    // Ignore errors due to being at a detached head.
  }

  spawnChecked("git", ["reset", "--hard", treeish], {
    cwd: dir,
    stdio: "inherit",
  });
}

function maybeDeleteGitLockFile(dir) {
  const lockFile = path.join(dir, ".git", "index.lock");
  if (fs.existsSync(lockFile)) {
    fs.unlinkSync(lockFile);
  }
}

export function gn() {
  return currentPlatform() == Platform.windows ? "gn.bat" : "gn";
}

function runGnGen() {
  spawnChecked(gn(), ["gen", "out/Release"], { stdio: "inherit" });
}

function gclient() {
  return currentPlatform() == Platform.windows ? "gclient.bat" : "gclient";
}

function runGclientSync() {
  spawnChecked(
    gclient(),
    ["sync", "-D", "--upstream", "--no-history", "--shallow"],
    {
      stdio: "inherit",
    }
  );
}

function updateRepo(repo, treeish) {
  log(`Updating ${repo} to ${treeish}`);
  // delete git lock file if it exists on Windows
  if (currentPlatform() == Platform.windows) {
    maybeDeleteGitLockFile(repo);
  }

  syncRepo(repo, treeish);
}

export function updateBackendRepo() {
  const backend = getBackendDir();
  const rev = fs.readFileSync("REPLAY_BACKEND_REV", "utf8").trim();
  updateRepo(backend, rev);
  // create a symlink to chromium in the backend checkout
  const chromiumRepoPath = process.cwd();
  const chromiumPathInBackend = path.join(backend, "chromium");
  if (fs.existsSync(chromiumPathInBackend)) {
    fs.unlinkSync(chromiumPathInBackend);
  }
  fs.symlinkSync(chromiumRepoPath, chromiumPathInBackend);
}

export function updateChromiumRepo() {
  const chromium = process.cwd();
  const branch = process.env["BUILDKITE_BRANCH"];
  updateRepo(chromium, `origin/${branch}`);

  runGclientSync();

  runGnGen();
}

export function getBackendDir() {
  return path.resolve(
    process.env.RECORD_REPLAY_BACKEND_DIR || path.join(__dirname, "..", "..")
  );
}
