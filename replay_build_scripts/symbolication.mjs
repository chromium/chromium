import readline from "readline";
import { toNumber, spawnChecked } from "./common.mjs";

export async function readSymbols(file, pdbFile) {
  const symbols = {};
  if (process.platform == "win32") {
    if (!pdbFile) {
      throw new Error("Need PDB file to read symbols on windows");
    }
    const textStart = getTextSectionAddress(pdbFile);

    const process = spawn(`${__dirname}\\..\\..\\lib\\llvm-pdbutil.exe`, [
      "dump",
      "-symbols",
      pdbFile,
    ]);
    // We use readline here instead of streamToLineIterator so that we can
    // keep the dependencies of this file to a minimum since this is also
    // used to build the linker and rebuilding the linker every time anything
    // in shared/ changes would be really slow.
    const lines = readline.createInterface({
      input: process.stdout,
      crlfDelay: Infinity,
    });
    let currentFunction;
    for await (const line of lines) {
      if (currentFunction) {
        const match = /addr = 0001:(\d+),/.exec(line);
        if (match) {
          const addr = textStart + +match[1];
          symbols[addr] = currentFunction;
        }
        currentFunction = undefined;
      } else if (line.includes("S_GPROC32") || line.includes("S_LPROC32")) {
        const backtick = line.indexOf("`");
        if (backtick != -1) {
          currentFunction = line.substring(backtick + 1, line.length - 1);
        }
      }
    }
  } else {
    const lines = spawnChecked("/usr/bin/nm", [file], { maxBuffer: 1e100 })
      .stdout.toString()
      .split("\n");
    for (const line of lines) {
      const arr = /^(.*?) [t|T|W] (.*)/.exec(line);
      if (arr) {
        try {
          symbols[toNumber(`0x${arr[1]}`)] = arr[2];
        } catch (e) {
          console.log(arr[2], e);
        }
      }
    }
  }
  return symbols;
}

// Get the start virtual address of the text section from a PDB file.
// Symbol addresses are relative to the start of this section.
function getTextSectionAddress(pdbFile) {
  const lines = spawnChecked(`${__dirname}\\..\\..\\lib\\llvm-pdbutil.exe`, [
    "dump",
    "-section-headers",
    pdbFile,
  ])
    .stdout.toString()
    .split("\n");

  let inTextSection = false;
  for (const line of lines) {
    if (line.includes(".text name")) {
      inTextSection = true;
    }
    const match = /([0-9A-F]+) virtual address/.exec(line);
    if (match) {
      if (!inTextSection) {
        throw new Error("Expected first section to be text section");
      }
      return parseInt(`0x${match[1]}`);
    }
  }
  throw new Error("Could not find start of text section");
}
