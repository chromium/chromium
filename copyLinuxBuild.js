// Copyright 2021 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Copy the build artifacts which are actually needed to a new directory. */

const fs = require("fs");
const { spawnSync } = require("child_process");

const srcDir = process.argv[2];
const dstDir = process.argv[3];
if (!srcDir || !dstDir) {
  console.log("Usage: node copyLinuxBuild.js srcDir dstDir");
  process.exit(1);
}

for (const file of fs.readdirSync(srcDir)) {
  if (shouldCopyFile(file)) {
    spawnSync("cp", [`${srcDir}/${file}`, dstDir], { stdio: "inherit" });
  }
}
spawnSync("cp", ["-R", `${srcDir}/locales`, dstDir], { stdio: "inherit" });

function shouldCopyFile(file) {
  const names = [
    "chrome",
    "chrome_crashpad_handler",
    "icudtl.dat",
    "v8_context_snapshot.bin",
    "vk_swiftshader_icd.json",
  ];
  if (names.includes(file)) {
    return true;
  }

  const extensions = [
    ".pak",
    ".pak.info",
    ".so",
    ".so.1",
  ];
  if (extensions.some(extension => file.endsWith(extension))) {
    return true;
  }

  return false;
}
