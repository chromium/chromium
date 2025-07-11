# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys
import os
import subprocess


def Main():
  parser = argparse.ArgumentParser()
  parser.add_argument('patch', help='path to the patch file to apply')
  parser.add_argument('input', help='path to the input file to patch')
  parser.add_argument('output', help='path to write the patched file')
  parsed = parser.parse_args()
  process = subprocess.run(
      ['patch', '-p1', '-i', parsed.patch, '-o', parsed.output, parsed.input],
      stdout=subprocess.PIPE, stderr=subprocess.PIPE)

  if process.returncode:
    sys.stderr.write(process.stderr.decode('utf-8'))
    sys.exit(process.returncode)


if __name__ == '__main__':
  sys.exit(Main())
