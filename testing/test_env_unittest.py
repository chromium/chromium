#!/usr/bin/env python
# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for test_env.py functionality.

Each unit test is launches python process that uses test_env.py
to launch another python process. Then signal handling and
propagation is tested. This similates how Swarming uses test_env.py.
"""

import os
import signal
import subprocess
import sys
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
TEST_SCRIPT = os.path.join(HERE, 'test_env_user_script.py')


def launch_process_windows(args):
  # The `universal_newlines` option is equivalent to `text` in Python 3.
  return subprocess.Popen(
      [sys.executable, TEST_SCRIPT] + args,
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      env=os.environ.copy(),
      creationflags=subprocess.CREATE_NEW_PROCESS_GROUP,
      universal_newlines=True)


def launch_process_nonwindows(args):
  # The `universal_newlines` option is equivalent to `text` in Python 3.
  return subprocess.Popen(
      [sys.executable, TEST_SCRIPT] + args,
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      env=os.environ.copy(),
      universal_newlines=True)


def read_subprocess_message(proc, starts_with):
  """Finds the value after first line prefix condition."""
  for line in proc.stdout:
    if line.startswith(starts_with):
      return line.rstrip().replace(starts_with, '')


def send_and_wait(proc, sig, sleep_time=0.3):
  """Sends a signal to subprocess."""
  time.sleep(sleep_time)  # gives process time to launch.
  os.kill(proc.pid, sig)
  proc.wait()


class SignalingWindowsTest(unittest.TestCase):

  def setUp(self):
    super(SignalingWindowsTest, self).setUp()
    if sys.platform != 'win32':
      self.skipTest('test only runs on Windows')

  def test_send_ctrl_break_event(self):
    proc = launch_process_windows([])
    send_and_wait(proc, signal.CTRL_BREAK_EVENT)
    sig = read_subprocess_message(proc, 'Signal :')
    self.assertEqual(sig, str(int(signal.SIGBREAK)))


class SignalingNonWindowsTest(unittest.TestCase):

  def setUp(self):
    super(SignalingNonWindowsTest, self).setUp()
    if sys.platform == 'win32':
      self.skipTest('test does not run on Windows')

  def test_send_sigterm(self):
    proc = launch_process_nonwindows([])
    send_and_wait(proc, signal.SIGTERM)
    sig = read_subprocess_message(proc, 'Signal :')
    self.assertEqual(sig, str(int(signal.SIGTERM)))

  def test_send_sigint(self):
    proc = launch_process_nonwindows([])
    send_and_wait(proc, signal.SIGINT)
    sig = read_subprocess_message(proc, 'Signal :')
    self.assertEqual(sig, str(int(signal.SIGINT)))


if __name__ == '__main__':
  unittest.main()
