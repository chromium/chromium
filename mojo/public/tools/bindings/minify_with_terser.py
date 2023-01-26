#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This utility minifies JS files with terser.
#
# Instance of 'node' has no 'RunNode' member (no-member)
# pylint: disable=no-member

import argparse
import os
import sys

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..', '..'))
_CWD = os.getcwd()
sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'node'))
import node
import node_modules


def MinifyFile(input_file, output_file):
  node.RunNode([
      node_modules.PathToTerser(), input_file, '--mangle', '--compress',
      '--comments', 'false', '--output', output_file
  ])


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--input', required=True)
  parser.add_argument('--output', required=True)
  args = parser.parse_args(argv)

  # Delete the output file if it already exists. It may be a sym link to the
  # input, because in non-optimized/pre-Terser builds the input file is copied
  # to the output location with gn copy().
  out_path = os.path.join(_CWD, args.output)
  if (os.path.exists(out_path)):
    os.remove(out_path)

  MinifyFile(os.path.join(_CWD, args.input), out_path)


if __name__ == '__main__':
  main(sys.argv[1:])
