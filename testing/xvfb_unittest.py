#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for xvfb.py functionality.

Each unit test is launching xvfb_test_script.py
through xvfb.py as a subprocess, then tests its expected output.
"""

import io
import os
import signal
import socket
import subprocess
import sys
import tempfile
import textwrap
import time
import unittest
from unittest import mock

import xvfb


TEST_FILE = __file__.replace('.pyc', '.py')
XVFB = TEST_FILE.replace('_unittest', '')
XVFB_TEST_SCRIPT = TEST_FILE.replace('_unittest', '_test_script')


def launch_process(args):
  """Launches a sub process to run through xvfb.py."""
  return subprocess.Popen(
    [XVFB, XVFB_TEST_SCRIPT] + args,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    env=os.environ.copy(),
  )


def read_subprocess_message(proc, starts_with):
  """Finds the value after first line prefix condition."""
  for line in proc.stdout.read().decode('utf-8').splitlines(True):
    if str(line).startswith(starts_with):
      return line.rstrip().replace(starts_with, '')


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

  def test_x11_environment_does_not_inherit_wayland_session(self):
    env = {
      'WAYLAND_DISPLAY': 'wayland-test',
      'WAYLAND_SOCKET': '10',
      'XDG_SESSION_TYPE': 'wayland',
    }
    with mock.patch.object(xvfb, '_run_with_x11', return_value=0) as run_x11:
      self.assertEqual(xvfb.run_executable(['test-command'], env), 0)
    run_x11.assert_called_once()
    self.assertNotIn('WAYLAND_DISPLAY', env)
    self.assertNotIn('WAYLAND_SOCKET', env)
    self.assertEqual(env['XDG_SESSION_TYPE'], 'x11')

  def test_mutter_supervisor_restarts_after_crash(self):
    fake_mutter = textwrap.dedent("""
      import resource
      import socket
      import sys
      resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
      s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
      s.bind(sys.argv[1])
      s.listen(8)
      while True:
        conn, _ = s.accept()
        conn.close()
    """)
    with tempfile.TemporaryDirectory() as tmpdir:
      sock_path = os.path.join(tmpdir, 'wayland-test')
      stderr = io.StringIO()
      with mock.patch('sys.stderr', stderr):
        supervisor = xvfb._MutterSupervisor(
          [sys.executable, '-c', fake_mutter, sock_path],
          os.environ.copy(),
          sock_path,
        )
        supervisor.start()
        try:
          first_pid = supervisor._proc.pid
          os.kill(first_pid, signal.SIGSEGV)
          deadline = time.monotonic() + 5
          while time.monotonic() < deadline:
            with supervisor._lock:
              restarted = supervisor._proc.pid != first_pid
            if restarted:
              break
            time.sleep(0.05)
          self.assertTrue(restarted)
          with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
            client.connect(sock_path)
        finally:
          supervisor.stop()
      self.assertFalse(os.path.exists(sock_path))
      output = stderr.getvalue()
      self.assertIn('mutter died with signal SIGSEGV', output)
      self.assertIn('Restarted mutter (restart 1)', output)

  def test_mutter_supervisor_startup_failure(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      sock_path = os.path.join(tmpdir, 'wayland-test')
      supervisor = xvfb._MutterSupervisor(
        [sys.executable, '-c', 'import sys; sys.exit(2)'],
        os.environ.copy(),
        sock_path,
      )
      with self.assertRaisesRegex(xvfb._ProcessError, 'exit code 2'):
        supervisor.start()

  def test_mutter_environment_isolates_wayland_display(self):
    env = {
      'XDG_RUNTIME_DIR': '/tmp/xdg-test',
      'WAYLAND_DISPLAY': 'wayland-outer',
      'WAYLAND_SOCKET': '10',
    }
    cmd = [
      './test_bin',
      '--no-xvfb',
      '--use-mutter',
      '--mutter-debug-logging',
    ]

    def fake_common(_, command, environment):
      return (True, command)

    with (
      mock.patch.object(xvfb, 'dbus_session'),
      mock.patch.object(xvfb, '_run_with_wayland_common', fake_common),
      mock.patch.object(xvfb, '_MutterSupervisor') as supervisor_cls,
      mock.patch.object(xvfb.test_env, 'run_executable', return_value=7) as run,
    ):
      rc = xvfb.run_executable(cmd, env)

    self.assertEqual(rc, 7)
    mutter_cmd, mutter_env, sock_path = supervisor_cls.call_args.args
    self.assertNotIn('WAYLAND_DISPLAY', mutter_env)
    self.assertNotIn('WAYLAND_SOCKET', mutter_env)
    self.assertEqual(mutter_env['G_MESSAGES_DEBUG'], 'libmutter')
    self.assertNotIn('G_MESSAGES_DEBUG', env)
    self.assertNotIn('WAYLAND_SOCKET', env)
    self.assertTrue(env['WAYLAND_DISPLAY'].startswith('wayland-xvfb-'))
    self.assertEqual(
      sock_path, os.path.join('/tmp/xdg-test', env['WAYLAND_DISPLAY'])
    )
    supervisor_cls.return_value.start.assert_called_once()
    supervisor_cls.return_value.stop.assert_called_once()
    run.assert_called_once_with(['./test_bin'], env, None, None)

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
    time.sleep(0.3)
    send_signal(self._proc, signal.SIGINT, 1)
    sig = read_subprocess_message(self._proc, 'Signal :')
    self.assertIsNotNone(sig)  # OpenBox likely failed to start
    self.assertEqual(int(sig), int(signal.SIGINT))

  def test_send_sigterm(self):
    self._proc = launch_process(['--sleep'])
    # Give time for subprocess to install signal handlers
    time.sleep(0.3)
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
