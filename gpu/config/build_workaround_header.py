#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""code generator for gpu workaround definitions"""

import os
import os.path
import sys
from optparse import OptionParser

_LICENSE = """// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"""

_DO_NOT_EDIT_WARNING = ("// This file is auto-generated from\n" +
  "//    //gpu/config/build_workaround_header.py\n" +
  "// DO NOT EDIT!\n\n")

def merge_files_into_workarounds(files):
  workarounds = set()
  for filename in files:
    with open(filename, 'r') as f:
      workarounds.update([workaround.strip() for workaround in f])
  return sorted(list(workarounds))


def write_header(filename, workarounds):
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


def main(argv):
  usage = "usage: %prog [options] file1 file2 file3 etc"
  parser = OptionParser(usage=usage)
  parser.add_option(
      "--output-file",
      dest="output_file",
      default="gpu_driver_bug_workaround_autogen.h",
      help="the name of the header file to write")

  (options, _) = parser.parse_args(args=argv)

  workarounds = merge_files_into_workarounds(parser.largs)
  write_header(options.output_file, workarounds)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
