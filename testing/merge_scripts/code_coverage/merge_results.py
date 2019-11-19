#!/usr/bin/python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Merge results from code coverage swarming runs.

This script merges code coverage profiles from multiple shards. It also merges
the test results of the shards.

It is functionally similar to merge_steps.py but it accepts the parameters
passed by swarming api.
"""

import argparse
import json
import logging
import os
import subprocess
import sys

import merge_lib as coverage_merger


def _MergeAPIArgumentParser(*args, **kwargs):
  """Parameters passed to this merge script, as per:
  https://chromium.googlesource.com/chromium/tools/build/+/master/scripts/slave/recipe_modules/swarming/resources/merge_api.py
  """
  parser = argparse.ArgumentParser(*args, **kwargs)
  parser.add_argument('--build-properties', help=argparse.SUPPRESS)
  parser.add_argument('--summary-json', help=argparse.SUPPRESS)
  parser.add_argument('--task-output-dir', help=argparse.SUPPRESS)
  parser.add_argument(
      '-o', '--output-json', required=True, help=argparse.SUPPRESS)
  parser.add_argument('jsons_to_merge', nargs='*', help=argparse.SUPPRESS)

  # Custom arguments for this merge script.
  parser.add_argument(
      '--additional-merge-script', help='additional merge script to run')
  parser.add_argument(
      '--additional-merge-script-args',
      help='JSON serialized string of args for the additional merge script')
  parser.add_argument(
      '--profdata-dir', required=True, help='where to store the merged data')
  parser.add_argument(
      '--llvm-profdata', required=True, help='path to llvm-profdata executable')
  parser.add_argument(
      '--java-coverage-dir', help='directory for Java coverage data')
  parser.add_argument(
      '--jacococli-path', help='path to jacococli.jar.')
  parser.add_argument(
      '--merged-jacoco-filename',
      help='filename used to uniquely name the merged exec file.')
  return parser


def main():
  desc = "Merge profraw files in <--task-output-dir> into a single profdata."
  parser = _MergeAPIArgumentParser(description=desc)
  params = parser.parse_args()

  if params.java_coverage_dir:
    if not params.jacococli_path:
      parser.error('--jacococli-path required when merging Java coverage')
    if not params.merged_jacoco_filename:
      parser.error(
          '--merged-jacoco-filename required when merging Java coverage')

    output_path = os.path.join(
        params.java_coverage_dir, '%s.exec' % params.merged_jacoco_filename)
    logging.info('Merging JaCoCo .exec files to %s', output_path)
    coverage_merger.merge_java_exec_files(
        params.task_output_dir, output_path, params.jacococli_path)

  # NOTE: The coverage data merge script must make sure that the profraw files
  # are deleted from the task output directory after merging, otherwise, other
  # test results merge script such as layout tests will treat them as json test
  # results files and result in errors.
  logging.info('Merging code coverage profraw data')
  invalid_profiles, counter_overflows = coverage_merger.merge_profiles(
      params.task_output_dir,
      os.path.join(params.profdata_dir, 'default.profdata'), '.profraw',
      params.llvm_profdata)

  # At the moment counter overflows overlap with invalid profiles, but this is
  # not guaranteed to remain the case indefinitely. To avoid future conflicts
  # treat these separately.
  if counter_overflows:
    with open(
        os.path.join(params.profdata_dir, 'profiles_with_overflows.json'),
        'w') as f:
      json.dump(counter_overflows, f)

  if invalid_profiles:
    with open(os.path.join(params.profdata_dir, 'invalid_profiles.json'),
              'w') as f:
      json.dump(invalid_profiles, f)

    # We don't want to invalidate shards in a CQ build, which we determine by
    # the existence of the 'patch_storage' property.
    build_properties = json.loads(params.build_properties)
    if not build_properties.get('patch_storage'):
      mark_invalid_shards(
          coverage_merger.get_shards_to_retry(invalid_profiles),
          params.jsons_to_merge)
  logging.info('Merging %d test results', len(params.jsons_to_merge))
  failed = False

  # If given, always run the additional merge script, even if we only have one
  # output json. Merge scripts sometimes upload artifacts to cloud storage, or
  # do other processing which can be needed even if there's only one output.
  if params.additional_merge_script:
    new_args = [
        '--build-properties',
        params.build_properties,
        '--summary-json',
        params.summary_json,
        '--task-output-dir',
        params.task_output_dir,
        '--output-json',
        params.output_json,
    ]

    if params.additional_merge_script_args:
      new_args += json.loads(params.additional_merge_script_args)

    new_args += params.jsons_to_merge

    args = [sys.executable, params.additional_merge_script] + new_args
    rc = subprocess.call(args)
    if rc != 0:
      failed = True
      logging.warning('Additional merge script %s exited with %s' %
                      (params.additional_merge_script, rc))
  elif len(params.jsons_to_merge) == 1:
    logging.info("Only one output needs to be merged; directly copying it.")
    with open(params.jsons_to_merge[0]) as f_read:
      with open(params.output_json, 'w') as f_write:
        f_write.write(f_read.read())
  else:
    logging.warning(
        "This script was told to merge %d test results, but no additional "
        "merge script was given.")

  return 1 if (failed or bool(invalid_profiles)) else 0


def mark_invalid_shards(bad_shards, jsons_to_merge):
  """Removes results json files from bad shards.

  This is needed so that the caller (e.g. recipe) knows to retry, or otherwise
  treat the tests in that shard as not having valid results. Note that this only
  removes the results from the local machine, as these are expected to remain in
  the shard's isolated output.

  Args:
    bad_shards: list of task_ids of the shards that are bad or corrupted.
    jsons_to_merge: The path to the jsons with the results of the tests.
  """
  if not bad_shards:
    return
  for f in jsons_to_merge:
    for task_id in bad_shards:
      if task_id in f:
        # Remove results json if it corresponds to a bad shard.
        os.remove(f)
        break


if __name__ == '__main__':
  logging.basicConfig(
      format='[%(asctime)s %(levelname)s] %(message)s', level=logging.INFO)
  sys.exit(main())
