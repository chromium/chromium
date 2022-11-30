#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fixup GCC-generated dependency files.

Modify GCC generated dependency files so they are more suitable for including
in a GNU Makefile.  Without the fixups, deleting or renaming headers can cause
the build to be broken.

See http://mad-scientist.net/make/autodep.html for more details of the problem.
"""

import argparse
import os
import sys

TAG_LINE = '# Updated by fix_deps.py\n'


class Error(Exception):
  pass


def ParseLine(line, new_target):
  """Parse one line of a GCC-generated deps file.

  Each line contains an optional target and then a list
  of space separated dependencies.  Spaces within filenames
  are escaped with a backslash.
  """
  filenames = []

  if new_target and ':' in line:
    line = line.split(':', 1)[1]

  line = line.strip()
  line = line.rstrip('\\')

  while True:
    # Find the next non-escaped space
    line = line.strip()
    pos = line.find(' ')
    while pos > 0 and line[pos-1] == '\\':
      pos = line.find(' ', pos+1)

    if pos == -1:
      filenames.append(line)
      break
    filenames.append(line[:pos])
    line = line[pos+1:]

  return filenames


def FixupDepFile(filename, output_filename=None):
  if not os.path.exists(filename):
    raise Error('File not found: %s' % filename)

  if output_filename is None:
    output_filename = filename

  outlines = [TAG_LINE]
  deps = []
  new_target = True
  with open(filename) as infile:
    for line in infile:
      if line == TAG_LINE:
        raise Error('Already processed: %s' % filename)
      outlines.append(line)
      deps += ParseLine(line, new_target)
      new_target = line.endswith('\\')

  # For every depenency found output a dummy target with no rules
  for dep in deps:
    outlines.append('%s:\n' % dep)

  with open(output_filename, 'w') as outfile:
    for line in outlines:
      outfile.write(line)


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-o', '--output', help='Output filename (defaults to '
                    'input name with .deps extension')
  parser.add_argument('-c', '--clean', action='store_true',
                    help='Remove input file after writing output')
  parser.add_argument('dep_file')
  options = parser.parse_args(argv)
  output_filename = options.output
  if not output_filename:
    output_filename = os.path.splitext(options.dep_file)[0] + '.deps'
  FixupDepFile(options.dep_file, output_filename)
  if options.clean and options.dep_file != output_filename:
    os.remove(options.dep_file)

  return 0


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except Error as e:
    sys.stderr.write('%s: %s\n' % (os.path.basename(__file__), e))
    sys.exit(1)
