#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This utility concatenates several files into one. On Unix-like systems
# it is equivalent to:
#   cat file1 file2 file3 ...files... > target
#
# The reason for writing a separate utility is that 'cat' is not available
# on all supported build platforms, but Python is, and hence this provides
# us with an easy and uniform way of doing this on all platforms.

# for py2/py3 compatibility
from __future__ import print_function

import optparse
import sys


def Concatenate(filenames):
  """Concatenate files.

  Args:
    files: Array of file names.
           The last name is the target; all earlier ones are sources.

  Returns:
    True, if the operation was successful.
  """
  if len(filenames) < 2:
    print("An error occurred generating %s:\nNothing to do." % filenames[-1])
    return False

  try:
    with open(filenames[-1], "wb") as target:
      for filename in filenames[:-1]:
        with open(filename, "rb") as current:
          target.write(current.read())
    return True
  except IOError as e:
    print("An error occurred when writing %s:\n%s" % (filenames[-1], e))
    return False


def main():
  parser = optparse.OptionParser()
  parser.set_usage("""Concatenate several files into one.
      Equivalent to: cat file1 ... > target.""")
  (_options, args) = parser.parse_args()
  sys.exit(0 if Concatenate(args) else 1)


if __name__ == "__main__":
  main()
