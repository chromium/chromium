// Copyright 2021 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copy the build artifacts which are actually needed to a new directory.
// Only used on linux and windows.

const fs = require("fs");
const path = require("path");
const { spawnSync } = require("child_process");

const srcDir = process.argv[2];
const dstDir = process.argv[3];
if (!srcDir || !dstDir) {
  console.log("Usage: node copyBuild.js srcDir dstDir");
  process.exit(1);
}

for (const file of fs.readdirSync(srcDir)) {
  if (shouldCopyFile(file)) {
    fs.cpSync(path.join(srcDir, file), path.join(dstDir, file));
  }
}
fs.cpSync(path.join(srcDir, "locales"), path.join(dstDir, "locales"), { recursive: true });

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
  if (names.includes(file)) {
    return true;
  }

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
  if (extensions.some(extension => file.endsWith(extension))) {
    return true;
  }

  return false;
}

function spawnChecked(cmd, args, options) {
  const prettyCmd = [cmd].concat(args).join(" ");
  console.error(prettyCmd);

  const rv = spawnSync(cmd, args, options);

  if (rv.status != 0 || rv.error) {
    console.error(rv.error);
    throw new Error(`Spawned process failed with exit code ${rv.status}`);
  }

  return rv;
}
