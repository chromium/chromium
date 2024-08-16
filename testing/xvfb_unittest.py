#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
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

# pylint: disable=super-with-arguments

TEST_FILE = __file__.replace('.pyc', '.py')
XVFB = TEST_FILE.replace('_unittest', '')
XVFB_TEST_SCRIPT = TEST_FILE.replace('_unittest', '_test_script')


def launch_process(args):
  """Launches a sub process to run through xvfb.py."""
  return subprocess.Popen([XVFB, XVFB_TEST_SCRIPT] + args,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT,
                          env=os.environ.copy())


# pylint: disable=inconsistent-return-statements
def read_subprocess_message(proc, starts_with):
  """Finds the value after first line prefix condition."""
  for line in proc.stdout.read().decode('utf-8').splitlines(True):
    if str(line).startswith(starts_with):
      return line.rstrip().replace(starts_with, '')


# pylint: enable=inconsistent-return-statements


def send_signal(proc, sig, sleep_time=0.3):
  """Sends a signal to subprocess."""
  time.sleep(sleep_time)  # gives process time to launch.
  os.kill(proc.pid, sig)
  proc.wait()


class XvfbLinuxTest(unittest.TestCase):

  def setUp(self):
    super(XvfbLinuxTest, self).setUp()
    if not sys.platform.startswith('linux'):
      self.skipTest('linux only test')
    self._procs = []

  def test_no_xvfb_display(self):
    self._procs.append(launch_process(['--no-xvfb']))
    self._procs[0].wait()
    display = read_subprocess_message(self._procs[0], 'Display :')
    self.assertEqual(display, os.environ.get('DISPLAY', 'None'))

  def test_xvfb_display(self):
    self._procs.append(launch_process([]))
    self._procs[0].wait()
    display = read_subprocess_message(self._procs[0], 'Display :')
    self.assertIsNotNone(display)  # Openbox likely failed to open DISPLAY
    self.assertNotEqual(display, os.environ.get('DISPLAY', 'None'))

  def test_no_xvfb_flag(self):
    self._procs.append(launch_process(['--no-xvfb']))
    self._procs[0].wait()

  def test_xvfb_flag(self):
    self._procs.append(launch_process([]))
    self._procs[0].wait()

  @unittest.skip('flaky; crbug.com/1320399')
  def test_xvfb_race_condition(self):
    self._procs = [launch_process([]) for _ in range(15)]
    for proc in self._procs:
      proc.wait()
    display_list = [
        read_subprocess_message(p, 'Display :') for p in self._procs
    ]
    for display in display_list:
      self.assertIsNotNone(display)  # Openbox likely failed to open DISPLAY
      self.assertNotEqual(display, os.environ.get('DISPLAY', 'None'))

  def tearDown(self):
    super(XvfbLinuxTest, self).tearDown()
    for proc in self._procs:
      if proc.stdout:
        proc.stdout.close()


class XvfbTest(unittest.TestCase):

  def setUp(self):
    super(XvfbTest, self).setUp()
    if sys.platform == 'win32':
      self.skipTest('non-win32 test')
    self._proc = None

  def test_send_sigint(self):
    self._proc = launch_process(['--sleep'])
    # Give time for subprocess to install signal handlers
    time.sleep(.3)
    send_signal(self._proc, signal.SIGINT, 1)
    sig = read_subprocess_message(self._proc, 'Signal :')
    self.assertIsNotNone(sig)  # OpenBox likely failed to start
    self.assertEqual(int(sig), int(signal.SIGINT))

  def test_send_sigterm(self):
    self._proc = launch_process(['--sleep'])
    # Give time for subprocess to install signal handlers
    time.sleep(.3)
    send_signal(self._proc, signal.SIGTERM, 1)
    sig = read_subprocess_message(self._proc, 'Signal :')
    self.assertIsNotNone(sig)  # OpenBox likely failed to start
    self.assertEqual(int(sig), int(signal.SIGTERM))

  def tearDown(self):
    super(XvfbTest, self).tearDown()
    if self._proc.stdout:
      self._proc.stdout.close()


if __name__ == '__main__':
  unittest.main()
