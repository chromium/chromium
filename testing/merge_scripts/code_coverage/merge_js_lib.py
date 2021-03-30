# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions to merge multiple JavaScript coverage files into one"""

import logging
import json
import os
import sys

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


def _get_coverage_paths(input_dir):
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
      if fn.endswith('.cov.json')
    ])
  return paths


def merge_coverage_files(coverage_dir, output_path):
  """Merge all coverages in the coverage dir into a single file.

  Args:
    coverage_dir (str): Path to all the raw JavaScript coverage files.
    output_path  (str): Path to the location to output merged coverage.
  """
  coverage_by_path = {}
  json_files = _get_coverage_paths(coverage_dir)

  if not json_files:
    logging.info('No JavaScript coverage files found in %s', coverage_dir)
    return

  for file_path in json_files:
    coverage_data = _parse_json_file(file_path)

    if 'result' not in coverage_data:
      raise RuntimeError('%r does not have a result field' %
                        json_file_path)

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
