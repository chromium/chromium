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
  if (["chrome", "icudtl.dat", "v8_context_snapshot.bin"].includes(file) ||
      file.endsWith(".pak") ||
      file.endsWith(".pak.info") ||
      file.endsWith(".so")) {
    spawnSync("cp", [`${srcDir}/${file}`, dstDir]);
  }
}
spawnSync("cp", ["-R", `${srcDir}/locales`, dstDir]);
