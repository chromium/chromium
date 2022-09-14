# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import shutil
import subprocess
import sys


def main():
  description = 'Packages WebKit build for Clusterfuzz.'
  parser = argparse.ArgumentParser(description=description)
  parser.add_argument('--output',
                    help='Name of the output file.')
  parser.add_argument('--webkit_build',
                      help='WebKit build directory to copy.')
  parser.add_argument('--clusterfuzz_script',
                      help='Clusterfuzz script to copy.')
  parser.add_argument('--clang_asan_library',
                      help='Clang ASan library to copy.')

  opts = parser.parse_args()

  if os.path.exists(opts.output):
    shutil.rmtree(opts.output)

  shutil.copytree(opts.webkit_build, opts.output, symlinks=True)
  shutil.copyfile(
        opts.clusterfuzz_script,
        os.path.join(opts.output,
                     os.path.basename(opts.clusterfuzz_script)))
  shutil.copyfile(
        opts.clang_asan_library,
        os.path.join(opts.output,
                     os.path.basename(opts.clang_asan_library)))

  zip_command = ['zip', '--symlinks', '-r', os.extsep.join([opts.output, 'zip']), opts.output]
  proc = subprocess.Popen(zip_command)
  proc.communicate()
  return proc.returncode


if __name__ == '__main__':
  sys.exit(main())
