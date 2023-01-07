# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple python wrapper so GN can run build-deb.sh."""
import os
import subprocess
import sys

REMOTING_USE_WAYLAND_OPT = "--remoting_use_wayland"


def main():
  this_dir = os.path.dirname(os.path.abspath(__file__))
  build_deb_script = os.path.join(this_dir, 'build-deb.sh')
  if (REMOTING_USE_WAYLAND_OPT in sys.argv):
    proc_env = os.environ.copy()
    proc_env["REMOTING_USE_WAYLAND"] = "1"
    args = sys.argv[1:]
    args.remove(REMOTING_USE_WAYLAND_OPT)
    proc = subprocess.Popen([build_deb_script] + args, stdout=subprocess.PIPE,
                            env=proc_env)
  else:
    proc = subprocess.Popen([build_deb_script] + sys.argv[1:],
                            stdout=subprocess.PIPE)
  out, _ = proc.communicate()
  sys.stdout.write(out.decode('utf8').strip())
  return proc.returncode


if __name__ == '__main__':
  sys.exit(main())
