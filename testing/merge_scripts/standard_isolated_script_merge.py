#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import sys

import merge_api
import results_merger


def StandardIsolatedScriptMerge(output_json, summary_json, jsons_to_merge):
  """Merge the contents of one or more results JSONs into a single JSON.

  Args:
    output_json: A path to a JSON file to which the merged results should be
      written.
    jsons_to_merge: A list of paths to JSON files that should be merged.
  """
  # summary.json is produced by swarming.py itself. We are mostly interested
  # in the number of shards.
  try:
    with open(summary_json) as f:
      summary = json.load(f)
  except (IOError, ValueError):
    print >> sys.stderr, (
        'summary.json is missing or can not be read',
        'Something is seriously wrong with swarming_client/ or the bot.')
    return 1

  missing_shards = []
  shard_results_list = []
  for index, result in enumerate(summary['shards']):
    output_path = None
    if result:
      output_path = find_shard_output_path(index, result.get('task_id'),
                                           jsons_to_merge)
    if not output_path:
      missing_shards.append(index)
      continue

    with open(output_path) as f:
      try:
        json_contents = json.load(f)
      except ValueError:
        raise ValueError('Failed to parse JSON from %s' % j)
      shard_results_list.append(json_contents)

  merged_results = results_merger.merge_test_results(shard_results_list)
  if missing_shards:
    merged_results['missing_shards'] = missing_shards
    if 'global_tags' not in merged_results:
      merged_results['global_tags'] = []
    merged_results['global_tags'].append('UNRELIABLE_RESULTS')

  with open(output_json, 'w') as f:
    json.dump(merged_results, f)

  return 0


def find_shard_output_path(index, task_id, jsons_to_merge):
  """Finds the shard matching the index/task-id.

  Args:
    index: The index of the shard to load data for, this is for old api.
    task_id: The directory of the shard to load data for, this is for new api.
    jsons_to_merge: A container of file paths for shards that emitted output.

  Returns:
    * The matching path, or None
  """
  # 'output.json' is set in swarming/api.py, gtest_task method.
  matching_json_files = [
      j for j in jsons_to_merge
      if (os.path.basename(j) == 'output.json' and
          (os.path.basename(os.path.dirname(j)) == str(index) or
           os.path.basename(os.path.dirname(j)) == task_id))]

  if not matching_json_files:
    print >> sys.stderr, 'shard %s test output missing' % index
    return None
  elif len(matching_json_files) > 1:
    print >> sys.stderr, 'duplicate test output for shard %s' % index
    return None

  return matching_json_files[0]


def main(raw_args):
  parser = merge_api.ArgumentParser()
  args = parser.parse_args(raw_args)
  return StandardIsolatedScriptMerge(
      args.output_json, args.summary_json, args.jsons_to_merge)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
