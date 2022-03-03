# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions to merge multiple JavaScript coverage files into one"""

import logging
import json
import os
import sys

_HERE_PATH = os.path.dirname(__file__)
_THIRD_PARTY_PATH = os.path.normpath(
    os.path.join(_HERE_PATH, '..', '..', '..', 'third_party'))
sys.path.append(os.path.join(_THIRD_PARTY_PATH, 'node'))
sys.path.append(os.path.join(_THIRD_PARTY_PATH, 'js_code_coverage'))
import node
import coverage_modules

logging.basicConfig(format='[%(asctime)s %(levelname)s] %(message)s',
                    level=logging.DEBUG)


def _parse_json_file(path):
  """Opens file and parses data into JSON

  Args:
    path (str): The path to a JSON file to parse.
  """
  with open(path, 'r') as json_file:
    return json.load(json_file)


def _peek_last(stack):
  """Returns the top element of stack or None"""
  return stack[-1] if stack else None


def _convert_to_disjoint_segments(ranges):
  """Converts a list of v8 CoverageRanges into a list of disjoint segments.

  A v8 CoverageRange is a JSON object that describes the start and end
  character offsets for a block of instrumented JavaScript code:
  https://chromedevtools.github.io/devtools-protocol/tot/Profiler/#type-CoverageRange
  CoverageRange is defined by the ranges field from a v8 FunctionCoverage:
  https://chromedevtools.github.io/devtools-protocol/tot/Profiler/#type-FunctionCoverage

  To compute the list of disjoint segments, we sort (must be a stable sort)
  the |ranges| list in ascending order by their startOffset. This
  has the effect of bringing CoverageRange groups closer together. Each
  group of CoverageRange's has a recursive relationship such that:
  - The first range in the group defines the character offsets for the
    function we are capturing coverage for
  - Children of this range identify unexcuted code unless they are
    also parents, in which case they continue the recursive relationship

  To give an example, consider the following arrow function:

  exports.test = arg => { return arg ? 'y' : 'n' }

  An invocation of test(true) would produce the following |ranges|

  [
    { "startOffset":  0, "endOffset": 48, "count": 1 }, // Range 1
    { "startOffset": 15, "endOffset": 48, "count": 1 }, // Range 2
    { "startOffset": 41, "endOffset": 46, "count": 0 }, // Range 3
  ]

  Range 1 identifies the entire script.
  Range 2 identifies the function from the arg parameter through
  to the closing brace
  Range 3 identifies that the code from offset [41, 46) was
  not executed.

  If we were to make the function calls, e.g. test(true); test(true);
  this would produce the following |ranges|

  [
    { "startOffset":  0, "endOffset": 48, "count": 1 }, // Range 1
    { "startOffset": 15, "endOffset": 48, "count": 2 }, // Range 2
    { "startOffset": 41, "endOffset": 46, "count": 0 }, // Range 3
  ]

  All the offsets are maintained, however the count on Range
  2 has increased while the count on Range 1 is unchanged. This
  shows another implicit assumption such that the inner most parent
  range count identifies the total invocation count.

  TODO(benreich): Write up more extensive documentation.

  Args:
    ranges (list): A list of v8 CoverageRange that have been
      merged from multiple FunctionCoverage. The order in which they
      appear in the original v8 coverage output must be maintained.

  Returns:
    A list of dictionaries where each entry is defined as:
      {
        count: Number of invocations of this range
        end: Exclusive character offset for the end of this range
      }
  """
  stack = []
  segments = []

  # pylint: disable=unsupported-assignment-operation
  def _append(end, count):
    """Append a new range segment to |segments|.

    If the top range on |segments| has the same ending as |end|
    return early, otherwise extend the segment if the same count
    exists.

    Args:
      end (number): The end character offset for the range
      count (number): The invocation count for the range
    """
    last = _peek_last(segments)
    if last is not None:
      if last['end'] == end:
        return

      if last['count'] == count:
        last['end'] = end
        return

    if end == 0:
      return

    segments.append({'end': end, 'count': count})
    return

  # Stable sort the range segments.
  ranges.sort(key=lambda entry: entry['startOffset'])

  for entry in ranges:
    top = _peek_last(stack)

    while top and top['endOffset'] <= entry['startOffset']:
      _append(top['endOffset'], top['count'])
      stack.pop()
      top = _peek_last(stack)

    top_count = 0 if not top else top['count']
    _append(entry['startOffset'], top_count)
    stack.append(entry)

  while stack:
    top = stack.pop()
    _append(top['endOffset'], top['count'])

  return segments
  # pylint: enable=unsupported-assignment-operation

# pylint: disable=unsupported-assignment-operation
def _merge_segments(segments_a, segments_b):
  """Merges 2 lists of disjoint segments into one

  Take in two lists that have been output by _convert_to_disjoint_segments
  and merge them into a single list. Any segments that are
  overlapping sum their invocation counts. If the overlap
  is partial, split the ranges into contiguous segments and
  assign the invocation counts appropriately.

  Args:
    segments_a (list): A list of disjoint segments.
    segments_b (list): A list of disjoint segments.

  Returns:
    A list of disjoint segments.
  """
  segments = []
  i = 0
  j = 0

  while i < len(segments_a) and j < len(segments_b):
    a = segments_a[i]
    b = segments_b[j]

    count = a.get('count', 0) + b.get('count', 0)
    end = min(a['end'], b['end'])
    last = _peek_last(segments)

    # Get the segment from the top of the stack and
    # extend the segment if the invocation counts match
    # otherwise push a new range segment onto the stack.
    if last is None or last['count'] != count:
      segments.append({'end': end, 'count': count})
    else:
      last['end'] = end

    if a['end'] <= b['end']:
      i += 1

    if a['end'] >= b['end']:
      j += 1

  while i < len(segments_a):
    segments.append(segments_a[i])
    i += 1

  while j < len(segments_b):
    segments.append(segments_b[j])
    j += 1

  return segments
# pylint: enable=unsupported-assignment-operation


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
      os.path.join(dir_path, fn) for fn in file_names
      if fn.endswith(suffix)
    ])
  return paths


def merge_coverage_files(coverage_dir, output_path):
  """Merge all coverages in the coverage dir into a single file.

  Args:
    coverage_dir (str): Path to all the raw JavaScript coverage files.
    output_path  (str): Path to the location to output merged coverage.
  """
  coverage_by_path = {}
  json_files = _get_paths_with_suffix(coverage_dir, '.cov.json')

  if not json_files:
    logging.info('No JavaScript coverage files found in %s', coverage_dir)
    return None

  for file_path in json_files:
    coverage_data = _parse_json_file(file_path)

    if 'result' not in coverage_data:
      raise RuntimeError('%r does not have a result field' %
                        file_path)

    for script_coverage in coverage_data['result']:
      script_url = script_coverage['url']

      # Ignore files with paths that have not been rewritten.
      # Files can rewrite paths by appending a //# sourceURL=
      # comment.
      if not script_url.startswith('//'):
        continue

      previous_coverage = coverage_by_path.get(script_url, [])

      ranges = []
      for function_coverage in script_coverage['functions']:
        for range_coverage in function_coverage['ranges']:
          ranges.append(range_coverage)

      disjoint_segments = _convert_to_disjoint_segments(ranges)
      merged_segments = _merge_segments(previous_coverage,
                                        disjoint_segments)

      coverage_by_path[script_url] = merged_segments

  with open(output_path, 'w') as merged_coverage_file:
    return merged_coverage_file.write(json.dumps(coverage_by_path))


def write_parsed_scripts(task_output_dir):
  """Extract parsed script contents and write back to original folder structure.

  Args:
    task_output_dir (str): The output directory for the sharded task. This will
        contain the raw JavaScript v8 parsed files that are identified by
        their ".js.json" suffix.

  Returns:
    The absolute file path to the raw parsed scripts or None if no parsed
    scripts were identified (or any of the raw data contains invalid JSON).
  """
  scripts = _get_paths_with_suffix(task_output_dir, '.js.json')
  output_dir = os.path.join(task_output_dir, 'parsed_scripts')

  if not scripts:
    return None

  for file_path in scripts:
    # TODO(crbug.com/1224786): Some of the raw script data is being saved with
    # a trailing curly brace leading to invalid JSON. Bail out if this is
    # encountered and ensure we log the file path.
    script_data = None
    try:
      script_data = _parse_json_file(file_path)
    except ValueError as e:
      logging.error('Failed to parse %s: %s', file_path, e)
      return None

    if any(key not in script_data for key in ('url', 'text')):
      logging.info('File %s is missing key url or text', file_path)
      continue

    if not script_data['url'].startswith('//'):
      continue

    source_path = os.path.normpath(script_data['url'].replace('//', ''))
    source_directory = os.path.join(output_dir, os.path.dirname(source_path))
    if not os.path.exists(source_directory):
      os.makedirs(source_directory)

    with open(os.path.join(output_dir, source_path), 'wb') as f:
      f.write(script_data['text'].encode('utf8'))

  return output_dir


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


def convert_raw_coverage_to_istanbul(
    raw_coverage_dirs, source_dir, task_output_dir):
  """Calls the node helper script convert_to_istanbul.js

  Args:
    raw_coverage_dirs (list): Directory that contains raw v8 code coverage.
    source_dir (str): Root directory containing the instrumented source.

  Raises:
    RuntimeError: If the underlying node command fails.
  """
  return node.RunNode(
      [os.path.join(_HERE_PATH, 'convert_to_istanbul.js'),
          '--source-dir', source_dir,
          '--output-dir', task_output_dir,
          '--raw-coverage-dirs', ' '.join(raw_coverage_dirs),
      ])

def merge_istanbul_reports(istanbul_coverage_dir, source_dir, output_file):
  """Merges all disparate istanbul reports into a single report.

  Args:
    istanbul_coverage_dir (str): Directory containing separate coverage files.
    source_dir (str): Directory containing instrumented source code.
    output_file (str): File path to output merged coverage.

  Raises:
    RuntimeError: If the underlying node command fails.
  """
  return node.RunNode(
      [coverage_modules.PathToNyc(),
          'merge', istanbul_coverage_dir,
          output_file,
          '--cwd', source_dir,
      ])
