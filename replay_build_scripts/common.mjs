import { spawnSync } from "child_process";

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
  windows: "windows"
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
