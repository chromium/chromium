#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import subprocess
import sys
import os

DESCRIPTION = '''Run the given JavaScript files through jscompile.'''
FILES_HELP = '''A list of Javascript files. The Javascript files should include
files that contain definitions of types or functions that are known to Chrome
but not to jscompile.'''
STAMP_HELP = 'Timestamp file to update on success.'

def checkJavascript(js_files):
  args = ['jscompile'] + js_files
  result = subprocess.call(args)
  return result == 0


def main():
  parser = argparse.ArgumentParser(description = DESCRIPTION)
  parser.add_argument('files', nargs = '+', help = FILES_HELP)
  parser.add_argument('--success-stamp', dest = 'success_stamp',
                      help = STAMP_HELP)
  options = parser.parse_args()

  js = []
  for file in options.files:
    name, extension = os.path.splitext(file)
    if extension == '.js':
      js.append(file)
    else:
      print >> sys.stderr, 'Unknown extension (' + extension + ') for ' + file
      return 1

  if not checkJavascript(js):
    return 1

  if options.success_stamp:
    with open(options.success_stamp, 'w'):
      os.utime(options.success_stamp, None)

  return 0

if __name__ == '__main__':
  sys.exit(main())
