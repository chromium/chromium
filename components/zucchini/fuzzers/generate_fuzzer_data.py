#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for generating new binary protobuf seeds for fuzzers.

Currently supports creating a single seed binary protobuf of the form
zucchini.fuzzers.FilePair.
"""

# Keep the existing two-space indentation to limit the scope of this change.
# pylint: disable=bad-indentation

import argparse
import logging
import os
import platform
import subprocess
import sys
import tempfile

import create_seed_file_pair

ABS_PATH = os.path.dirname(os.path.abspath(__file__))
ABS_TESTDATA_PATH = os.path.join(ABS_PATH, 'testdata')


def parse_args(argv=None):
  """Parses arguments from command-line."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--raw', help='Whether to use Raw Zucchini.',
                      action='store_true')
  parser.add_argument('old_file', help='Old file to generate/apply patch.')
  parser.add_argument('new_file', help='New file to generate patch from.')
  parser.add_argument('output_file', help='File to write binary protobuf to.')
  return parser.parse_args(argv)


def _cleanup_temporary_directory(temp_dir):
  """Removes the temporary patch without hiding a child-process failure."""
  try:
    temp_dir.cleanup()
  except OSError:
    logging.exception('Failed to clean temporary patch directory.')
    return False
  return True


def generate_seed(zucchini_path,
                  protoc_path,
                  old_file,
                  new_file,
                  output_file,
                  is_raw):
  """Generates a new patch and binary encodes a protobuf pair."""
  zuc_cmd = [zucchini_path, '-gen', '--v=-1']
  if is_raw:
    zuc_cmd.append('-raw')
  # Avoid a with statement so cleanup errors do not replace child return codes.
  temp_dir = tempfile.TemporaryDirectory(  # pylint: disable=consider-using-with
      prefix='zucchini-seed-')
  patch_file = os.path.join(temp_dir.name, 'patch.zuc')
  returncode = 1
  try:
    # Generate a new patch.
    result = subprocess.run(zuc_cmd + [old_file, new_file, patch_file],
                            check=False)
    if result.returncode:
      logging.error('Patch generation failed for (%s, %s)', old_file,
                    new_file)
      returncode = result.returncode
    else:
      # Binary encode the protobuf pair.
      returncode = create_seed_file_pair.create_seed_file_pair(
          protoc_path, old_file, patch_file, output_file)
  finally:
    cleanup_succeeded = _cleanup_temporary_directory(temp_dir)

  if not cleanup_succeeded and returncode == 0:
    return 1
  return returncode


def main(argv=None):
  args = parse_args(argv)
  suffix = '.exe' if platform.system() == 'Windows' else ''
  tools_dir = os.getcwd()
  return generate_seed(
      os.path.abspath(os.path.join(tools_dir, 'zucchini' + suffix)),
      os.path.abspath(os.path.join(tools_dir, 'protoc' + suffix)),
      os.path.join(ABS_TESTDATA_PATH, args.old_file),
      os.path.join(ABS_TESTDATA_PATH, args.new_file),
      os.path.abspath(args.output_file),
      args.raw)


if __name__ == '__main__':
  sys.exit(main())
