#!/usr/bin/env python
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Merge javascript results from code-coverage/pgo swarming runs.

Called by merge_results.py
"""

import argparse
import logging
import os
import sys

import merge_js_lib as javascript_merger


def _MergeAPIArgumentParser(*args, **kwargs):
  parser = argparse.ArgumentParser(*args, **kwargs)
  parser.add_argument('--task-output-dir', help=argparse.SUPPRESS)
  parser.add_argument('--javascript-coverage-dir',
                      help='directory for JavaScript coverage data')
  parser.add_argument('--chromium-src-dir',
                      help='directory for chromium/src checkout')
  parser.add_argument('--build-dir',
                      help='directory for the build directory in chromium/src')
  return parser


def main():
  parser = _MergeAPIArgumentParser()
  params = parser.parse_args()

  parsed_scripts = javascript_merger.write_parsed_scripts(
      params.task_output_dir)

  # Ensure JavaScript coverage dir exists.
  if not os.path.exists(params.javascript_coverage_dir):
    os.makedirs(params.javascript_coverage_dir)

  if not parsed_scripts:
    logging.warning('No parsed scripts written')
    return

  logging.info('Raw parsed scripts written out to %s', parsed_scripts)
  coverage_dirs = javascript_merger.get_raw_coverage_dirs(
      params.task_output_dir)
  logging.info('Identified directories containing coverage %s', coverage_dirs)

  try:
    logging.info('Converting raw coverage to istanbul')
    javascript_merger.convert_raw_coverage_to_istanbul(coverage_dirs,
                                                       parsed_scripts,
                                                       params.task_output_dir)

    istanbul_coverage_dir = os.path.join(params.task_output_dir, 'istanbul')
    output_dir = os.path.join(istanbul_coverage_dir, 'merged')
    os.makedirs(output_dir, exist_ok=True)

    coverage_file_path = os.path.join(output_dir, 'coverage.json')
    logging.info('Merging istanbul reports to %s', coverage_file_path)
    javascript_merger.merge_istanbul_reports(istanbul_coverage_dir,
                                             parsed_scripts, coverage_file_path)

    logging.info('Excluding uninteresting lines from coverage')
    javascript_merger.exclude_uninteresting_lines(coverage_file_path)

    logging.info('Remapping all paths relative to the src dir and removing any '
                 'files in the out dir')
    javascript_merger.remap_paths_to_relative(coverage_file_path,
                                              params.chromium_src_dir,
                                              params.build_dir)

    logging.info('Creating lcov report at %s', params.javascript_coverage_dir)
    javascript_merger.generate_coverage_reports(output_dir,
                                                params.javascript_coverage_dir)
  except RuntimeError as e:
    logging.warning('Failed executing istanbul tasks: %s', e)


if __name__ == '__main__':
  logging.basicConfig(format='[%(asctime)s %(levelname)s] %(message)s',
                      level=logging.INFO)
  sys.exit(main())
