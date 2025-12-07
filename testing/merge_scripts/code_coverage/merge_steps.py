#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script merges code coverage profiles from multiple steps."""

import argparse
import logging
import sys

import merge_lib as merger


def _merge_steps_argument_parser(*args, **kwargs):
  parser = argparse.ArgumentParser(*args, **kwargs)
  parser.add_argument('--input-dir', required=True, help=argparse.SUPPRESS)
  parser.add_argument('--output-file',
                      required=True,
                      help='where to store the merged data')
  parser.add_argument('--llvm-profdata',
                      required=True,
                      help='path to llvm-profdata executable')
  parser.add_argument(
      '--profdata-filename-pattern',
      default='.*',
      help='regex pattern of profdata filename to merge for current test type. '
      'If not present, all profdata files will be merged.')
  parser.add_argument('--sparse',
                      action='store_true',
                      dest='sparse',
                      help='run llvm-profdata with the sparse flag.')
  parser.add_argument(
      '--profile-merge-timeout',
      default=3600,
      help='Timeout (sec) for the call to merge profiles. Defaults to 3600.')
  parser.add_argument(
      '--weight',
      action='append',
      default=[],
      help='The weight to use for a particular benchmark. Format '
      'is benchmark:weight. Matching of benchmark is done using ends with.')
  return parser


def main():
  desc = 'Merge profdata files in <--input-dir> into a single profdata.'
  parser = _merge_steps_argument_parser(description=desc)
  params = parser.parse_args()

  weights = {}
  for name_and_weight in params.weight:
    parts = name_and_weight.split(':')
    if len(parts) != 2:
      logging.error('Invalid weight:\n%r', name_and_weight)
      return 1
    weights[parts[0]] = parts[1]

  # counter overflow profiles should be logged as warnings as part of the
  # merger.merge_profiles call.
  invalid_profiles, _ = merger.merge_profiles(
      params.input_dir,
      params.output_file,
      '.profdata',
      params.llvm_profdata,
      params.profdata_filename_pattern,
      sparse=params.sparse,
      merge_timeout=params.profile_merge_timeout,
      weights=weights)
  if invalid_profiles:
    logging.error('Invalid profiles were generated:\n%r', invalid_profiles)
    return 1

  return 0


if __name__ == '__main__':
  logging.basicConfig(format='[%(asctime)s %(levelname)s] %(message)s',
                      level=logging.INFO)
  sys.exit(main())
