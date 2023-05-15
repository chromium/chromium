import { spawnSync } from "child_process";
import * as fs from "fs";
import * as path from "path";

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

export function updateRepo() {
  const chromium = process.cwd();

  const branch = process.env["BUILDKITE_BRANCH"];

  syncRepo(chromium, `origin/${branch}`);

  const deps = getChromiumDeps();

  syncRepo(path.join(chromium, "v8"), deps.v8);

  syncRepo(path.join(chromium, "third_party", "skia"), deps.skia);

  syncRepo(path.join(chromium, "third_party", "webrtc"), deps.webrtc);

  syncRepo(
    path.join(chromium, "third_party", "boringssl", "src"),
    deps.boringssl
  );
}

function getChromiumDeps() {
  const text = fs.readFileSync("DEPS", "utf8");
  let results = {
    v8: "",
    skia: "",
    webrtc: "",
    boringssl: "",
  };

  let match = /'v8_revision': '(.*?)'/.exec(text);
  assert(match, "Could not find V8 revision");
  results.v8 = match[1];

  match = /'skia_revision': '(.*?)'/.exec(text);
  assert(match, "Could not find skia revision");
  results.skia = match[1];

  match =
    /'https:\/\/github.com\/replayio\/chromium-webrtc.git' \+ '@' \+ '(.*?)'/.exec(
      text
    );
  assert(match, "Could not find webrtc revision");
  results.webrtc = match[1];

  match = /'boringssl_revision': '(.*?)'/.exec(text);
  assert(match, "Could not find boringssl revision");
  results.boringssl = match[1];

  return results;
}
