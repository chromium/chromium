import fs from "fs";
import { getArtifactDir } from "./common.mjs";

function main() {
  fs.rmSync(getArtifactDir(), { force: true, recursive: true });
}

main();
