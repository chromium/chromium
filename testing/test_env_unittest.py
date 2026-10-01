#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for test_env.py and its generated script_test wrapper.

The signal tests launch Python through test_env.py to simulate Swarming.
The wrapper test checks Windows argument and exit-code forwarding.
"""

import json
import os
import signal
import subprocess
import sys
import tempfile
import time
import unittest

if sys.platform == 'win32':
  try:
    import win32api
    import win32con
    import win32process
  except ImportError:
    win32api = None

HERE = os.path.dirname(os.path.abspath(__file__))
TEST_SCRIPT = os.path.join(HERE, 'test_env_user_script.py')
GENERATOR_SCRIPT = os.path.normpath(
  os.path.join(HERE, '..', 'build', 'util', 'generate_wrapper.py')
)


def launch_process_windows(args):
  # The `universal_newlines` option is equivalent to `text` in Python 3.
  return subprocess.Popen(
    [sys.executable, TEST_SCRIPT] + args,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    env=os.environ.copy(),
    creationflags=subprocess.CREATE_NEW_PROCESS_GROUP,
    universal_newlines=True,
  )


def launch_process_nonwindows(args):
  # The `universal_newlines` option is equivalent to `text` in Python 3.
  return subprocess.Popen(
    [sys.executable, TEST_SCRIPT] + args,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    env=os.environ.copy(),
    universal_newlines=True,
  )


def read_subprocess_message(proc, starts_with):
  """Finds the value after first line prefix condition."""
  for line in proc.stdout:
    if line.startswith(starts_with):
      return line.rstrip().replace(starts_with, '')


def send_and_wait(proc, sig, sleep_time=0.6):
  """Sends a signal to subprocess."""
  time.sleep(sleep_time)  # gives process time to launch.
  os.kill(proc.pid, sig)
  proc.wait()


class SignalingWindowsTest(unittest.TestCase):
  def setUp(self):
    super().setUp()
    if sys.platform != 'win32':
      self.skipTest('test only runs on Windows')

  def test_send_ctrl_break_event(self):
    proc = launch_process_windows([])
    send_and_wait(proc, signal.CTRL_BREAK_EVENT)
    sig = read_subprocess_message(proc, 'Signal :')
    # This test is flaky because it relies on the child process starting quickly
    # "enough", which it fails to do sometimes. This is tracked by
    # https://crbug.com/1335123 and it is hoped that increasing the timeout will
    # reduce the flakiness.
    self.assertEqual(
      sig,
      str(int(signal.SIGBREAK)),
    )

  def test_job_object_kills_leaked_child_process(self):
    proc = launch_process_windows(['--spawn-leaked-child'])
    leaked_pid_str = read_subprocess_message(proc, 'Leaked PID:')
    proc.wait()
    self.assertIsNotNone(leaked_pid_str)
    leaked_pid = int(leaked_pid_str)
    time.sleep(0.2)

    if not win32api:
      return

    try:
      hproc = win32api.OpenProcess(
        win32con.PROCESS_QUERY_LIMITED_INFORMATION, False, leaked_pid
      )
      if hproc:
        exit_code = win32process.GetExitCodeProcess(hproc)
        win32api.CloseHandle(hproc)
        # 259 is STILL_ACTIVE; process should be terminated.
        self.assertNotEqual(exit_code, 259)
    except Exception:
      # OpenProcess failing indicates the PID is no longer valid (process dead).
      pass


class GeneratedWrapperTest(unittest.TestCase):
  def setUp(self):
    super().setUp()
    temp_dir = tempfile.TemporaryDirectory(prefix='generated wrapper ')
    self.addCleanup(temp_dir.cleanup)
    self.temp_dir = temp_dir.name
    probe = os.path.join(self.temp_dir, 'probe.py')
    with open(probe, 'w', encoding='utf-8') as probe_file:
      probe_file.write(
        'import json\n'
        'import sys\n'
        'print("WRAPPER_ARGV=" + json.dumps(sys.argv[1:]))\n'
        'sys.exit(int(sys.argv[1]))\n'
      )
    self.wrapper = self._generate_wrapper('run wrapper.bat', 'probe.py')

  def _generate_wrapper(self, name, executable):
    wrapper = os.path.join(self.temp_dir, name)
    subprocess.run(
      [
        sys.executable,
        GENERATOR_SCRIPT,
        '--executable',
        executable,
        '--wrapper-script',
        wrapper,
        '--output-directory',
        self.temp_dir,
        '--script-language',
        'batch',
      ],
      cwd=self.temp_dir,
      check=True,
      timeout=60,
    )
    return wrapper

  def _check_arguments_and_exit_code(self, command):
    forwarded = [
      '--isolated-script-test-filter='
      ':chromium-bidi!pytest:tests/:'
      'test_resultsink_repeat.py#test_case[paired!marker!value]',
      ':chromium-bidi!pytest:tests/script/:test_serialization.py#'
      "test_serialization_function[new Error('Woops!')-expected_serialized18]",
      'unpaired!bang',
      'argument with spaces & ampersand',
      '*.test.js',
    ]
    for exit_code in (0, 1, 5, 7):
      with self.subTest(exit_code=exit_code):
        expected_argv = [str(exit_code)] + forwarded
        result = subprocess.run(
          command + expected_argv,
          cwd=self.temp_dir,
          capture_output=True,
          text=True,
          check=False,
          timeout=60,
        )
        self.assertEqual(
          result.returncode, exit_code, result.stdout + result.stderr
        )
        marker = next(
          (
            line.removeprefix('WRAPPER_ARGV=')
            for line in result.stdout.splitlines()
            if line.startswith('WRAPPER_ARGV=')
          ),
          None,
        )
        self.assertIsNotNone(marker, result.stdout + result.stderr)
        self.assertEqual(json.loads(marker), expected_argv)

  def test_python_entry_point(self):
    # Check the batch/Python header under Python even on non-Windows hosts.
    # This does not exercise cmd.exe's argument or exit-status handling.
    self._check_arguments_and_exit_code([sys.executable, '-x', self.wrapper])

  @unittest.skipUnless(sys.platform == 'win32', 'test only runs on Windows')
  def test_preserves_arguments_and_exit_code(self):
    self._check_arguments_and_exit_code([self.wrapper])

  @unittest.skipUnless(sys.platform == 'win32', 'test only runs on Windows')
  def test_nested_wrappers_preserve_arguments_and_exit_code(self):
    # Also check the outer wrapper's process status: a generated suite wrapper
    # must not hide failures reported by its child.
    outer_wrapper = self._generate_wrapper(
      'run outer wrapper.bat', '@WrappedPath(run wrapper.bat)'
    )
    self._check_arguments_and_exit_code([outer_wrapper])


class SignalingNonWindowsTest(unittest.TestCase):
  def setUp(self):
    super().setUp()
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
