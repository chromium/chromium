# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates a list of all files in a directory.

This script takes in a directory and an output file name as input.
It then reads the directory and creates a list of all file names
in that directory.  The list is written to the output file.
There is also an option to pass in '-p' or '--pattern'
which will check each file name against a regular expression
pattern that is passed in.  Only files which match the regex
will be written to the list.
"""

from __future__ import print_function

import os
import re
import sys

from cStringIO import StringIO
from optparse import OptionParser

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "pylib"))

from mojom.generate.generator import  WriteFile


def main():
  parser = OptionParser()
  parser.add_option('-d', '--directory', help='Read files from DIRECTORY')
  parser.add_option('-o', '--output', help='Write list to FILE')
  parser.add_option('-p',
                    '--pattern',
                    help='Only reads files that name matches PATTERN',
                    default=".")
  (options, _) = parser.parse_args()
  pattern = re.compile(options.pattern)
  files = [f for f in os.listdir(options.directory) if pattern.match(f)]

  stream = StringIO()
  for f in files:
    print(f, file=stream)

  WriteFile(stream.getvalue(), options.output)
  stream.close()

if __name__ == '__main__':
  sys.exit(main())
