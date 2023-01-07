// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Takes raw v8 coverage files and converts to IstanbulJS
 * compliant coverage files.
 */

// Relative path to the node modules.
const NODE_MODULES = [
  '..', '..', '..', 'third_party', 'js_code_coverage', 'node_modules'];

const {createHash} = require('crypto');
const {join, normalize} = require('path');
const {readdir, readFile, writeFile, mkdir} = require('fs').promises;
const V8ToIstanbul = require(join(...NODE_MODULES, 'v8-to-istanbul'));
const {ArgumentParser} = require(join(...NODE_MODULES, 'argparse'));

/**
 * Extracts the raw coverage data from the v8 coverage reports and converts
 * them into IstanbulJS compliant reports.
 * @param {string} coverageDirectory Directory containing the raw v8 output.
 * @param {string} instrumentedDirectoryRoot Directory containing the source
 *    files where the coverage was instrumented from.
 * @param {string} outputDir Directory to store the istanbul coverage reports.
 * @param {!Map<string, string>} urlToPathMap A mapping of URL observed during
 *    test execution to the on-disk location created in previous steps.
 */
async function extractCoverage(
    coverageDirectory, instrumentedDirectoryRoot, outputDir, urlToPathMap) {
  const coverages = await readdir(coverageDirectory);

  for (const fileName of coverages) {
    if (!fileName.endsWith('.cov.json'))
      continue;

    const filePath = join(coverageDirectory, fileName);
    const contents = await readFile(filePath, 'utf-8');

    const {result: scriptCoverages} = JSON.parse(contents);
    if (!scriptCoverages)
      throw new Error(`result key missing for file: ${filePath}`);

    for (const coverage of scriptCoverages) {
      if (!urlToPathMap[coverage.url])
        continue;

      const instrumentedFilePath =
          join(instrumentedDirectoryRoot, urlToPathMap[coverage.url]);
      const converter = V8ToIstanbul(instrumentedFilePath);
      await converter.load();
      converter.applyCoverage(coverage.functions);
      const convertedCoverage = converter.toIstanbul();

      const jsonString = JSON.stringify(convertedCoverage);
      await writeFile(
          join(outputDir, createSHA1HashFromFileContents(jsonString) + '.json'),
          jsonString);
    }
  }
}

/**
 * Helper function to provide a unique file name for resultant istanbul reports.
 * @param {string} str File contents
 * @return {string} A sha1 hash to be used as a file name.
 */
function createSHA1HashFromFileContents(contents) {
  return createHash('sha1').update(contents).digest('hex');
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
  for (const coverageDir of args.raw_coverage_dirs)
    await extractCoverage(
        coverageDir, args.source_dir, outputDir, urlToPathMap);

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
