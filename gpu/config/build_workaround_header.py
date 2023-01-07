#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""code generator for gpu workaround definitions"""

import argparse
import os
import sys
import typing

_LICENSE = """// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"""

_DO_NOT_EDIT_WARNING = ("// This file is auto-generated from\n" +
  "//    //gpu/config/build_workaround_header.py\n" +
  "// DO NOT EDIT!\n\n")

def merge_files_into_workarounds(files: typing.List[str]) -> typing.List[str]:
  workarounds = set()
  for filename in files:
    with open(filename, 'r') as f:
      workarounds.update([workaround.strip() for workaround in f])
  return sorted(list(workarounds))


def write_header(filename: str, workarounds: typing.List[str]) -> None:
  max_workaround_len = len(max(workarounds, key=len))

  with open(filename, 'w') as f:
    f.write(_LICENSE)
    f.write(_DO_NOT_EDIT_WARNING)

    indent = '  '
    macro = 'GPU_OP'

    # length of max string passed to write + 1
    max_len = len(indent) + len(macro) + 1 + max_workaround_len + 1 + 1
    write = lambda line: f.write(line + ' ' * (max_len - len(line)) + '\\\n')

    write('#define GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)')
    for w in workarounds:
      write(indent + macro + '(' + w.upper() + ',')
      write(indent + ' ' * (len(macro) + 1) + w + ')')

    # one extra line to consume the the last \
    f.write('// The End\n')


def main():
  parser = argparse.ArgumentParser(
      description='Generate GPU workaround definitions')
  parser.add_argument(
      "--output-file",
      default="gpu_driver_bug_workaround_autogen.h",
      help="the name of the header file to write")
  parser.add_argument(
      'files',
      nargs='+',
      help='1 or more files to process')

  args = parser.parse_args()

  workarounds = merge_files_into_workarounds(args.files)
  write_header(args.output_file, workarounds)


if __name__ == '__main__':
  sys.exit(main())
