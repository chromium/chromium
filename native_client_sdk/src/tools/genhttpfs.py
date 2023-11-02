#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script generates a manifest file for nacl_io's HTTP file-system.
Files and directory paths are specified on the command-line.  The names
with glob and directories are recursed to form a list of files.

For each file, the mode bits, size and path relative to the CWD are written
to the output file which is stdout by default.
"""

import argparse
import glob
import os
import sys
import urllib

class Error(Exception):
  pass


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-C', '--srcdir',
                      help='Change directory.', dest='srcdir', default=None)
  parser.add_argument('-o', '--output',
                      help='Output file name.', dest='output', default=None)
  parser.add_argument('-v', '--verbose',
                      help='Verbose output.',  dest='verbose',
                      action='store_true')
  parser.add_argument('-r', '--recursive',
                      help='Recursive search.', action='store_true')
  parser.add_argument('paths', nargs='+')
  options = parser.parse_args(args)

  if options.output:
    outfile = open(options.output, 'w')
  else:
    outfile = sys.stdout

  if options.srcdir:
    os.chdir(options.srcdir)

  # Generate a set of unique file names bases on the input globs
  fileset = set()
  for fileglob in options.paths:
    filelist = glob.glob(fileglob)
    if not filelist:
      raise Error('Could not find match for "%s".\n' % fileglob)
    for filename in filelist:
      if os.path.isfile(filename):
        fileset |= set([filename])
        continue
      if os.path.isdir(filename) and options.recursive:
        for root, _, files in os.walk(filename):
          fileset |= set([os.path.join(root, name) for name in files])
        continue
      raise Error('Can not handle path "%s".\n' % filename)

  cwd = os.path.abspath(os.getcwd())
  cwdlen = len(cwd)
  for filename in sorted(fileset):
    relname = os.path.abspath(filename)
    if cwd not in relname:
      raise Error('%s is not relative to CWD %s.\n' % filename, cwd)
    relname = relname[cwdlen:]
    stat = os.stat(filename)
    mode = '-r--'
    name = urllib.quote(relname)
    outfile.write('%s %d %s\n' % (mode, stat.st_size, name))

  return 0


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except Error, e:
    sys.stderr.write('%s: %s\n' % (os.path.basename(__file__), e))
    sys.exit(1)
