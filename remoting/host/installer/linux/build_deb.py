# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple python wrapper so GN can run build-deb.sh."""
import os
import subprocess
import sys


def main():
  this_dir = os.path.dirname(os.path.abspath(__file__))
  build_deb_script = os.path.join(this_dir, 'build-deb.sh')
  proc = subprocess.Popen([build_deb_script] + sys.argv[1:],
                          stdout=subprocess.PIPE)
  out, _ = proc.communicate()
  sys.stdout.write(out.decode('utf8').strip())
  return proc.returncode


if __name__ == '__main__':
  sys.exit(main())
