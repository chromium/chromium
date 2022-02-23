#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Merge results from code-coverage/pgo swarming runs.

This script merges code-coverage/pgo profiles from multiple shards. It also
merges the test results of the shards.

It is functionally similar to merge_steps.py but it accepts the parameters
passed by swarming api.
"""

import argparse
import json
import logging
import os
import subprocess
import sys

import merge_lib as profile_merger

def _MergeAPIArgumentParser(*args, **kwargs):
  """Parameters passed to this merge script, as per:
  https://chromium.googlesource.com/chromium/tools/build/+/main/scripts/slave/recipe_modules/swarming/resources/merge_api.py
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
  parser.add_argument('--test-target-name', help='test target name')
  parser.add_argument(
      '--java-coverage-dir', help='directory for Java coverage data')
  parser.add_argument(
      '--jacococli-path', help='path to jacococli.jar.')
  parser.add_argument(
      '--merged-jacoco-filename',
      help='filename used to uniquely name the merged exec file.')
  parser.add_argument(
      '--javascript-coverage-dir',
      help='directory for JavaScript coverage data')
  parser.add_argument(
      '--merged-js-cov-filename', help='filename to uniquely identify merged '
                                       'json coverage data')
  parser.add_argument(
      '--per-cl-coverage',
      action='store_true',
      help='set to indicate that this is a per-CL coverage build')
  parser.add_argument(
      '--sparse',
      action='store_true',
      dest='sparse',
      help='run llvm-profdata with the sparse flag.')
  # (crbug.com/1091310) - IR PGO is incompatible with the initial conversion
  # of .profraw -> .profdata that's run to detect validation errors.
  # Introducing a bypass flag that'll merge all .profraw directly to .profdata
  parser.add_argument(
      '--skip-validation',
      action='store_true',
      help='skip validation for good raw profile data. this will pass all '
           'raw profiles found to llvm-profdata to be merged. only applicable '
           'when input extension is .profraw.')
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
    profile_merger.merge_java_exec_files(
        params.task_output_dir, output_path, params.jacococli_path)

  failed = False

  if params.javascript_coverage_dir:
    current_dir = os.path.dirname(__file__)
    merge_js_results_script = os.path.join(current_dir, 'merge_js_results.py')
    args = [
      sys.executable,
      merge_js_results_script,
      '--task-output-dir',
      params.task_output_dir,
      '--javascript-coverage-dir',
      params.javascript_coverage_dir,
      '--merged-js-cov-filename',
      params.merged_js_cov_filename,
    ]

    rc = subprocess.call(args)
    if rc != 0:
      failed = True
      logging.warning('%s exited with %s' %
                      (merge_js_results_script, rc))

  # Name the output profdata file name as {test_target}.profdata or
  # default.profdata.
  output_prodata_filename = (params.test_target_name or 'default') + '.profdata'

  # NOTE: The profile data merge script must make sure that the profraw files
  # are deleted from the task output directory after merging, otherwise, other
  # test results merge script such as layout tests will treat them as json test
  # results files and result in errors.
  invalid_profiles, counter_overflows = profile_merger.merge_profiles(
      params.task_output_dir,
      os.path.join(params.profdata_dir, output_prodata_filename), '.profraw',
      params.llvm_profdata,
      sparse=params.sparse,
      skip_validation=params.skip_validation)

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
        'This script was told to merge test results, but no additional merge '
        'script was given.')

  return 1 if (failed or bool(invalid_profiles)) else 0


if __name__ == '__main__':
  logging.basicConfig(
      format='[%(asctime)s %(levelname)s] %(message)s', level=logging.INFO)
  sys.exit(main())
