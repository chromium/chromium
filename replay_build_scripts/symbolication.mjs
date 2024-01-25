import readline from "readline";
import { spawn } from "child_process";
import * as path from "path";

import { toNumber, getBackendDir } from "./common.mjs";

export async function readSymbols(file, pdbFile) {
  let symbols = {};
  if (process.platform == "win32") {
    if (!pdbFile) {
      throw new Error("Need PDB file to read symbols on windows");
    }

    const pdbPath = path.join(getBackendDir(), "lib", "symextract.exe");
    const process = spawn(pdbPath, ["read_symbols", pdbFile]);
    process.on("error", (error) => {
      console.error(`spawn error: ${error}`);
    });

    const lines = readline.createInterface({
      input: process.stdout,
      crlfDelay: Infinity,
    });
    let allLines = [];
    for await (const line of lines) {
      allLines.push(line);
    }

    // TODO: this just converts a string to an object, and then
    // immediately converts it back (in the caller.)  Fix this.
    symbols = JSON.parse(allLines.join(''));
  } else {
    const nmProcess = spawn("/usr/bin/nm", [file], { maxBuffer: 1e100 });

    nmProcess.on("error", (error) => {
      console.error(`spawn error: ${error}`);
    });

    let stdout = "";
    nmProcess.stdout.on("data", (data) => {
      stdout += data;
    });

    await new Promise((resolve, reject) => {
      nmProcess.on("exit", resolve);
      nmProcess.on("error", reject);
    });

    const lines = stdout.split("\n");
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
// eslint-disable-next-line no-unused-vars
async function getTextSectionAddress(pdbFile) {
  const pdbPath = path.join(getBackendDir(), "lib", "llvm-pdbutil.exe");
  const pdbProcess = spawn(pdbPath, ["dump", "-section-headers", pdbFile]);

  pdbProcess.on("error", (error) => {
    console.error(`spawn error: ${error}`);
  });

  let stdout = "";
  pdbProcess.stdout.on("data", (data) => {
    stdout += data;
  });

  await new Promise((resolve, reject) => {
    pdbProcess.on("exit", resolve);
    pdbProcess.on("error", reject);
  });

  const lines = stdout.toString().split("\n");

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
