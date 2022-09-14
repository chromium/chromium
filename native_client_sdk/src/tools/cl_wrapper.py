# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper script for MSVC's cl.exe that filters out filename.

When cl.exe is run by 'make' we want to be behave more like
gcc and be silent by default. There seems to be no flag which
tells cl.exe to suppress the name of the file its compiling
so we use a wrapper script to filter its output.

This was inspired by ninja's msvc wrapper:
src/msvc_helper-win32.cc:CLParser::FilterInputFilename
"""

import os
import subprocess
import sys


def main(args):
  p = subprocess.Popen(['cl.exe'] + args, stdout=subprocess.PIPE)
  for line in p.stdout:
    extension = os.path.splitext(line.strip())[1]
    if extension.lower() not in ('.c', '.cpp', '.cxx', '.cc'):
      sys.stdout.write(line)
  return p.wait()


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
