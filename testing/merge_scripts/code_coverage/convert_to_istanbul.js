// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Takes raw v8 coverage files and converts to IstanbulJS
 * compliant coverage files.
 */

import {mkdir, readFile} from 'node:fs/promises';
import {join} from 'node:path';
import {Worker} from 'node:worker_threads';
import {ArgumentParser} from '../../../third_party/js_code_coverage/node_modules/argparse/argparse.js';

function createWorker(coverageDir, sourceDir, outputDir, urlToPathMap) {
  return new Promise(function(resolve, reject) {
    const worker = new Worker(join(import.meta.dirname, 'coverage_worker.js'), {
      workerData: {
        coverageDir: coverageDir,
        sourceDir: sourceDir,
        outputDir: outputDir,
        urlToPathMap: urlToPathMap
      },
    });
    worker.on('message', (data) => {
      resolve(data);
    });
    worker.on('error', (msg) => {
      reject(`An error ocurred: ${msg}`);
    });
  });
}

/**
 * The entry point to the function to enable the async functionality throughout.
 * @param {Object} args The parsed CLI arguments.
 * @return {!Promise<string>} Directory containing istanbul reports.
 */
async function main(args) {
  const urlToPathMapFile =
      await readFile(join(args.source_dir, 'parsed_scripts.json'));
  const urlToPathMap = JSON.parse(urlToPathMapFile.toString());
  const outputDir = join(args.output_dir, 'istanbul')
  await mkdir(outputDir, {recursive: true});

  const workerPromises = [];
  for (const coverageDir of args.raw_coverage_dirs) {
    workerPromises.push(
        createWorker(coverageDir, args.source_dir, outputDir, urlToPathMap));
  }

  const results = await Promise.all(workerPromises);
  console.log(`Result from workers: ${results}`)
  return outputDir;
}

const parser = new ArgumentParser({
  description: 'Converts raw v8 coverage into IstanbulJS compliant files.',
});

parser.add_argument('-s', '--source-dir', {
  required: true,
  help: 'Root directory where source files live. The corresponding ' +
      'coverage files must refer to these files. Currently source ' +
      'maps are not supported.',
});
parser.add_argument('-o', '--output-dir', {
  required: true,
  help: 'Root directory to output all the converted istanbul coverage ' +
      'reports.',
});
parser.add_argument('-c', '--raw-coverage-dirs', {
  required: true,
  nargs: '*',
  help: 'Directory that contains the raw v8 coverage files (files ' +
      'ending in .cov.json)',
});

const args = parser.parse_args();
main(args)
    .then(outputDir => {
      console.log(`Successfully converted from v8 to IstanbulJS: ${outputDir}`);
    })
    .catch(error => {
      console.error(error);
      process.exit(1);
    });
