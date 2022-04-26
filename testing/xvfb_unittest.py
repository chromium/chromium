#!/usr/bin/env python
# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for xvfb.py functionality.

Each unit test is launching xvfb_test_script.py
through xvfb.py as a subprocess, then tests its expected output.
"""

import os
import signal
import subprocess
import sys
import time
import unittest


TEST_FILE = __file__.replace('.pyc', '.py')
XVFB = TEST_FILE.replace('_unittest', '')
XVFB_TEST_SCRIPT = TEST_FILE.replace('_unittest', '_test_script')


def launch_process(args):
  """Launches a sub process to run through xvfb.py."""
  return subprocess.Popen(
      [XVFB, XVFB_TEST_SCRIPT] + args, stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT, env=os.environ.copy())


def read_subprocess_message(proc, starts_with):
  """Finds the value after first line prefix condition."""
  for line in proc.stdout:
    if line.startswith(starts_with):
      return line.rstrip().replace(starts_with, '')


def send_signal(proc, sig, sleep_time=0.3):
  """Sends a signal to subprocess."""
  time.sleep(sleep_time)  # gives process time to launch.
  os.kill(proc.pid, sig)
  proc.wait()


class XvfbLinuxTest(unittest.TestCase):

  def setUp(self):
    super(XvfbLinuxTest, self).setUp()
    if sys.platform != 'linux2':
      self.skipTest('linux only test')

  def test_no_xvfb_display(self):
    proc = launch_process(['--no-xvfb'])
    proc.wait()
    display = read_subprocess_message(proc, 'Display :')
    self.assertEqual(display, os.environ.get('DISPLAY', 'None'))

  def test_xvfb_display(self):
    proc = launch_process([])
    proc.wait()
    display = read_subprocess_message(proc, 'Display :')
    self.assertIsNotNone(display)
    self.assertNotEqual(display, os.environ.get('DISPLAY', 'None'))

  def test_no_xvfb_flag(self):
    proc = launch_process(['--no-xvfb'])
    proc.wait()

  def test_xvfb_flag(self):
    proc = launch_process([])
    proc.wait()

  def test_xvfb_race_condition(self):
    proc_list = [launch_process([]) for _ in range(15)]
    for proc in proc_list:
      proc.wait()
    display_list = [read_subprocess_message(p, 'Display :') for p in proc_list]
    for display in display_list:
      self.assertIsNotNone(display)
      self.assertNotEqual(display, os.environ.get('DISPLAY', 'None'))


class XvfbTest(unittest.TestCase):

  def setUp(self):
    super(XvfbTest, self).setUp()
    if sys.platform == 'win32':
      self.skipTest('non-win32 test')

  def test_send_sigint(self):
    proc = launch_process(['--sleep'])
    send_signal(proc, signal.SIGINT, 1)
    sig = read_subprocess_message(proc, 'Signal :')
    self.assertEqual(sig, str(signal.SIGINT))

  def test_send_sigterm(self):
    proc = launch_process(['--sleep'])
    send_signal(proc, signal.SIGTERM, 1)
    sig = read_subprocess_message(proc, 'Signal :')
    self.assertEqual(sig, str(signal.SIGTERM))

if __name__ == '__main__':
  unittest.main()
