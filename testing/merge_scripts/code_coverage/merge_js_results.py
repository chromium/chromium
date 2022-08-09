#!/usr/bin/env python
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Merge javascript results from code-coverage/pgo swarming runs.

Called by merge_results.py
"""

import argparse
import json
import logging
import os
import subprocess
import sys

import merge_lib as profile_merger
import merge_js_lib as javascript_merger

def _MergeAPIArgumentParser(*args, **kwargs):
  parser = argparse.ArgumentParser(*args, **kwargs)
  parser.add_argument('--task-output-dir', help=argparse.SUPPRESS)
  parser.add_argument(
      '--javascript-coverage-dir',
      help='directory for JavaScript coverage data')
  parser.add_argument(
      '--merged-js-cov-filename', help='filename to uniquely identify merged '
                                       'json coverage data')
  return parser


def main():
  parser = _MergeAPIArgumentParser()
  params = parser.parse_args()

  if not params.merged_js_cov_filename:
    parser.error('--merged-js-cov-filename required when merging '
                 'JavaScript coverage')

  parsed_scripts = javascript_merger.write_parsed_scripts(
      params.task_output_dir)
  if parsed_scripts:
    logging.info('Raw parsed scripts written out to %s', parsed_scripts)
    coverage_dirs = javascript_merger.get_raw_coverage_dirs(
        params.task_output_dir)
    logging.info(
        'Identified directories containing coverage %s', coverage_dirs)

    try:
      logging.info('Converting raw coverage to istanbul')
      javascript_merger.convert_raw_coverage_to_istanbul(
          coverage_dirs, parsed_scripts, params.task_output_dir)

      istanbul_coverage_dir = os.path.join(params.task_output_dir, 'istanbul')
      output_dir = os.path.join(istanbul_coverage_dir, 'merged')
      os.makedirs(output_dir)

      coverage_file_path = os.path.join(output_dir, 'coverage.json')
      logging.info('Merging istanbul reports to %s', coverage_file_path)
      javascript_merger.merge_istanbul_reports(
          istanbul_coverage_dir, parsed_scripts, coverage_file_path)
    except RuntimeError as e:
      logging.warn('Failed executing istanbul tasks: %s', e)

  # Ensure JavaScript coverage dir exists.
  if not os.path.exists(params.javascript_coverage_dir):
    os.makedirs(params.javascript_coverage_dir)

  output_path = os.path.join(params.javascript_coverage_dir,
      '%s_javascript.json' % params.merged_js_cov_filename)
  logging.info('Merging v8 coverage output to %s', output_path)
  javascript_merger.merge_coverage_files(params.task_output_dir, output_path)

if __name__ == '__main__':
  logging.basicConfig(
      format='[%(asctime)s %(levelname)s] %(message)s', level=logging.INFO)
  sys.exit(main())
