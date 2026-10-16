#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for use in test_env unittests."""

from __future__ import print_function
import signal
import sys
import time


def print_signal(sig, *_args):
  print('Signal :{}'.format(sig))


if __name__ == '__main__':
  if '--spawn-leaked-child' in sys.argv:
    import subprocess

    child = subprocess.Popen(
      [sys.executable, '-c', 'import time; time.sleep(30)']
    )
    if sys.platform == 'win32':
      import psutil
      import win32api
      import win32con
      import win32process

      # Suspend the child's thread so cleanup_child_processes() exercises
      # ResumeThread() before killing the leaked child process.
      for t in psutil.Process(child.pid).threads():
        hthread = win32api.OpenThread(
          win32con.THREAD_SUSPEND_RESUME, False, t.id
        )
        try:
          win32process.SuspendThread(hthread)
        finally:
          win32api.CloseHandle(hthread)
    print('Leaked PID:{}'.format(child.pid))
    sys.stdout.flush()
    sys.exit(42)
  signal.signal(signal.SIGTERM, print_signal)
  signal.signal(signal.SIGINT, print_signal)
  if sys.platform == 'win32':
    signal.signal(signal.SIGBREAK, print_signal)
  time.sleep(2)  # gives process time to receive signal.
