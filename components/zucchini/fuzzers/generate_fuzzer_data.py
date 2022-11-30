#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for generating new binary protobuf seeds for fuzzers.

Currently supports creating a single seed binary protobuf of the form
zucchini.fuzzer.FilePair.
"""

import argparse
import hashlib
import logging
import os
import platform
import subprocess
import sys

ABS_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__)))
ABS_TESTDATA_PATH = os.path.join(ABS_PATH, 'testdata')

def parse_args():
  """Parses arguments from command-line."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--raw', help='Whether to use Raw Zucchini.',
                      action='store_true')
  parser.add_argument('old_file', help='Old file to generate/apply patch.')
  parser.add_argument('new_file', help='New file to generate patch from.')
  parser.add_argument('patch_file', help='Patch filename to use.')
  parser.add_argument('output_file', help='File to write binary protobuf to.')
  return parser.parse_args()


def gen(old_file, new_file, patch_file, output_file, is_raw, is_win):
  """Generates a new patch and binary encodes a protobuf pair."""
  # Create output directory if missing.
  output_dir = os.path.dirname(output_file)
  if not os.path.exists(output_dir):
    os.makedirs(output_dir)

  # Handle Windows executable names.
  zucchini = 'zucchini'
  protoc = 'protoc'
  if is_win:
    zucchini += '.exe'
    protoc += '.exe'

  zuc_cmd = [os.path.abspath(zucchini), '-gen']
  if is_raw:
    zuc_cmd.append('-raw')
  # Generate a new patch.
  ret = subprocess.call(zuc_cmd + [old_file, new_file, patch_file],
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE)
  if ret:
    logging.error('Patch generation failed for ({}, {})'.format(old_file,
                                                                new_file))
    return ret
  # Binary encode the protobuf pair.
  ret = subprocess.call([sys.executable,
                         os.path.join(ABS_PATH, 'create_seed_file_pair.py'),
                         os.path.abspath(protoc), old_file, patch_file,
                         output_file])
  os.remove(patch_file)
  return ret


def main():
  args = parse_args()
  return gen(os.path.join(ABS_TESTDATA_PATH, args.old_file),
             os.path.join(ABS_TESTDATA_PATH, args.new_file),
             os.path.abspath(args.patch_file),
             os.path.abspath(args.output_file),
             args.raw,
             platform.system() == 'Windows')


if __name__ == '__main__':
  sys.exit(main())

