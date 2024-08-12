# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions to merge multiple JavaScript coverage files into one"""

import base64
import logging
import json
import os
import sys

_HERE_PATH = os.path.dirname(__file__)
_THIRD_PARTY_PATH = os.path.normpath(
    os.path.join(_HERE_PATH, '..', '..', '..', 'third_party'))
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..'))

# //third_party/node imports.
sys.path.append(os.path.join(_THIRD_PARTY_PATH, 'node'))
import node

# //third_party/js_code_coverage imports.
sys.path.append(os.path.join(_THIRD_PARTY_PATH, 'js_code_coverage'))
import coverage_modules

logging.basicConfig(format='[%(asctime)s %(levelname)s] %(message)s',
                    level=logging.DEBUG)

_PREFIXES_TO_CHECK = ['//', 'import ', '/*', '*']


def _parse_json_file(path):
  """Opens file and parses data into JSON

  Args:
    path (str): The path to a JSON file to parse.
  """
  with open(path, 'r') as json_file:
    # Some JSON files erroroneously end with double curly brace, prefer to
    # strip it out instead of throwing an error message.
    json_string = json_file.read()
    if json_string[0] == '{' and json_string[-2:] == '}}':
      logging.warning('Found additional trailing curly brace for path: %s',
                      path)
      return json.loads(json_string[:-1])
    return json.loads(json_string)


def _get_paths_with_suffix(input_dir, suffix):
  """Gets all JSON files in the input directory.

  Args:
    input_dir (str): The path to recursively search for
        JSON files.

  Returns:
    A list of absolute file paths.
  """
  paths = []
  for dir_path, _sub_dirs, file_names in os.walk(input_dir):
    paths.extend([
        os.path.join(dir_path, fn) for fn in file_names if fn.endswith(suffix)
    ])
  return paths


def write_parsed_scripts(task_output_dir, source_dir=_SRC_PATH):
  """Extract parsed script contents and write back to original folder
  structure.

  Args:
    task_output_dir (str): The output directory for the sharded task. This will
        contain the raw JavaScript v8 parsed files that are identified by
        their ".js.json" suffix.

  Returns:
    The absolute file path to the raw parsed scripts or None if no parsed
    scripts were identified (or any of the raw data contains invalid JSON).
  """
  _SOURCEMAPPING_DATA_URL_PREFIX = 'data:application/json;base64,'

  scripts = _get_paths_with_suffix(task_output_dir, '.js.json')
  output_dir = os.path.join(task_output_dir, 'parsed_scripts')

  # The original file is extracted from the inline sourcemaps, this
  # information is not available from the coverage data. So we have to
  # maintain a URL to path map to ensure the coverage data knows the original
  # source location.
  url_to_path_map = {}

  if not scripts:
    return None

  for file_path in scripts:
    script_data = None
    try:
      script_data = _parse_json_file(file_path)
    except ValueError as e:
      logging.error('Failed to parse %s: %s', file_path, e)
      return None

    if any(key not in script_data for key in ('url', 'text', 'sourceMapURL')):
      logging.info('File %s is missing key url, text or sourceMapURL',
                   file_path)
      continue

    # TODO(crbug.com/40242180): For now we exclude any sourcemaps that are 0
    # length and also that don't begin with a data URL designation.
    if len(script_data['sourceMapURL']) == 0 or not script_data[
        'sourceMapURL'].startswith(_SOURCEMAPPING_DATA_URL_PREFIX):
      continue

    decoded_sourcemap = base64.b64decode(script_data['sourceMapURL'].replace(
        _SOURCEMAPPING_DATA_URL_PREFIX, ''))
    json_sourcemap = json.loads(decoded_sourcemap)
    if len(json_sourcemap['sources']) == 0:
      logging.warning('File %s has a valid sourcemap with no sources',
                      file_path)
      continue

    for source_idx in range(len(json_sourcemap['sources'])):
      source_path = os.path.relpath(
          os.path.normpath(
              os.path.join(json_sourcemap['sourceRoot'],
                           json_sourcemap['sources'][source_idx])), source_dir)
      source_directory = os.path.join(output_dir, os.path.dirname(source_path))
      if not os.path.exists(source_directory):
        os.makedirs(source_directory)

      with open(os.path.join(output_dir, source_path), 'wb') as f:
        f.write(script_data['text'].encode('utf8'))

      # Only write the first instance of the sources to the map.
      # Sourcemaps require stability in their indexing as the mapping
      # derived are based on the index location of the file in the
      # "sources" and "sourcesContent" fields. Therefore the first index
      # of the "sources" field will be the first file that was encountered
      # during source map generation, i.e. this should be the actual
      # chromium/src original file.
      if script_data['url'] not in url_to_path_map:
        url_to_path_map[script_data['url']] = source_path

  if not url_to_path_map:
    return None

  with open(os.path.join(output_dir, 'parsed_scripts.json'),
            'w+',
            encoding='utf-8') as f:
    json.dump(url_to_path_map, f)

  return output_dir


def should_exclude(line_contents):
  """Whether we exclude the line from coverage map."""
  line_contents = line_contents.strip()
  # Exclude empty lines.
  if line_contents == '':
    return True

  # Exclude comments and imports.
  for prefix in _PREFIXES_TO_CHECK:
    if line_contents.startswith(prefix):
      return True

  return False


def exclude_uninteresting_lines(coverage_file_path):
  """Removes lines from Istanbul coverage reports that correspond to lines in
  the source file that are empty. These lines provide no additional coverage
  information and in fact inflate the coverage metrics.

  Args:
    coverage_file_path (str): The path to the merged coverage.json file.
  """
  with open(coverage_file_path, 'r+') as f:
    coverage = json.load(f)

    def exclude_line(coverage_map, key):
      """Exclude an individual line from the coverage map. This relies on
            the key 'statementMap' which maintains a map of statements to lines
            as well as the key 's' which contains the invocation counts of each
            line.
            """
      del coverage_map['statementMap'][key]
      del coverage_map['s'][key]

    for file_path in coverage:
      istanbul_coverage = coverage[file_path]
      lines = []
      with open(file_path) as fd:
        lines = fd.readlines()

      # Force list of the keys to allow removal of items whilst iterating.
      for key in list(istanbul_coverage['statementMap']):
        statement_map = istanbul_coverage['statementMap'][key]
        line_num = statement_map['start']['line']

        assert statement_map['start']['line'] == statement_map['end']['line']

        if should_exclude(lines[line_num - 1]):
          exclude_line(istanbul_coverage, key)
          continue

    # Overwrite the current coverage file with new contents.
    f.seek(0)
    f.truncate()
    json.dump(coverage, f)


def remap_paths_to_relative(coverage_file_path, chromium_src_dir, build_dir):
  """Remap paths to be relative to the chromium_src_dir.

  Args:
    coverage_file_path (str): The path to the merged coverage.json file.
    chromium_src_dir (str): The absolute location to chromium/src.
    build_dir (str): The absolute path to the output dir in chromium/src.
  """
  with open(coverage_file_path, 'r+') as f:
    coverage_json = json.load(f)
    excluded_paths = 0
    remapped_paths = 0

    for key in list(coverage_json.keys()):

      if key.startswith(build_dir):
        del coverage_json[key]
        excluded_paths += 1
        continue

      if not key.startswith(chromium_src_dir):
        del coverage_json[key]
        excluded_paths += 1
        continue

      relative_src_path = os.path.relpath(key,
                                          chromium_src_dir).replace('\\', '/')
      value = coverage_json[key]
      value['path'] = relative_src_path
      coverage_json[relative_src_path] = value
      del coverage_json[key]
      remapped_paths += 1

    logging.info('Remapped %s paths', remapped_paths)
    logging.info('Excluded %s paths', excluded_paths)

    # Overwrite the current coverage file with new contents.
    f.seek(0)
    f.truncate()
    json.dump(coverage_json, f)


def get_raw_coverage_dirs(task_output_dir):
  """Returns a list of directories containing raw v8 coverage.

  Args:
    task_output_dir (str): The output directory for the sharded task. This will
        contain the raw JavaScript v8 coverage files that are identified by
        their ".cov.json" suffix.
  """
  coverage_directories = set()
  for dir_path, _sub_dirs, file_names in os.walk(task_output_dir):
    for name in file_names:
      if name.endswith('.cov.json'):
        coverage_directories.add(dir_path)
        continue

  return coverage_directories


def convert_raw_coverage_to_istanbul(raw_coverage_dirs, source_dir,
                                     task_output_dir):
  """Calls the node helper script convert_to_istanbul.js

  Args:
    raw_coverage_dirs (list): Directory that contains raw v8 code coverage.
    source_dir (str): Root directory containing the instrumented source.

  Raises:
    RuntimeError: If the underlying node command fails.
  """
  stdout = node.RunNode([
      os.path.join(_HERE_PATH, 'convert_to_istanbul.js'),
      '--source-dir',
      source_dir,
      '--output-dir',
      task_output_dir,
      '--raw-coverage-dirs',
      *raw_coverage_dirs,
  ])
  logging.info(stdout)


def merge_istanbul_reports(istanbul_coverage_dir, source_dir, output_file):
  """Merges all disparate istanbul reports into a single report.

  Args:
    istanbul_coverage_dir (str): Directory containing separate coverage files.
    source_dir (str): Directory containing instrumented source code.
    output_file (str): File path to output merged coverage.

  Raises:
    RuntimeError: If the underlying node command fails.
  """
  return node.RunNode([
      coverage_modules.PathToNyc(),
      'merge',
      istanbul_coverage_dir,
      output_file,
      '--cwd',
      source_dir,
  ])


def generate_coverage_reports(coverage_file_dir, output_dir):
  """Generate a LCOV report.

  Args:
    coverage_file_dir (str): Directory containing the coverage.json file.
    output_dir (str): Directory to output the reports.
  """
  return node.RunNode([
      coverage_modules.PathToNyc(),
      'report',
      '--temp-dir',
      coverage_file_dir,
      '--reporter',
      'lcov',
      '--report-dir',
      output_dir,
      '--exclude-after-remap',
      'false',
  ])
