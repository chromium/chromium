# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest
from unittest import mock

from pyfakefs import fake_filesystem_unittest  # pylint: disable=import-error

from gpu_tests.util import wayland_server

# pylint: disable=protected-access


class WaylandServerUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()
    self._orig_env = os.environ.copy()

  def tearDown(self):
    os.environ.clear()
    os.environ.update(self._orig_env)

  def testFindCompositorBinaryExplicit(self):
    custom_bin = os.path.join(os.path.sep, 'custom', 'bin', 'mutter')
    self.fs.CreateFile(custom_bin)
    server = wayland_server.WaylandServer(compositor_binary=custom_bin)
    self.assertEqual(server._FindCompositorBinary(), custom_bin)

  def testFindCompositorBinaryBuildDir(self):
    build_dir = os.path.join(os.path.sep, 'build', 'out', 'Release')
    expected_bin = os.path.join(build_dir, 'mutter')
    os.environ['CHROMIUM_OUTPUT_DIR'] = build_dir
    self.fs.CreateFile(expected_bin)
    os.chmod(expected_bin, 0o755)
    server = wayland_server.WaylandServer()
    self.assertEqual(server._FindCompositorBinary(), expected_bin)

  def testFindCompositorBinaryPath(self):
    mutter_bin = os.path.join(os.path.sep, 'usr', 'bin', 'mutter')
    with mock.patch('shutil.which', return_value=mutter_bin):
      server = wayland_server.WaylandServer()
      self.assertEqual(server._FindCompositorBinary(), mutter_bin)

  def testFindCompositorBinaryNone(self):
    with mock.patch('shutil.which', return_value=None):
      server = wayland_server.WaylandServer()
      self.assertIsNone(server._FindCompositorBinary())

  @mock.patch('subprocess.Popen')
  def testStartAndStopMutterSuccess(self, mock_popen):
    mock_proc = mock.MagicMock()
    mock_proc.poll.return_value = None

    mutter_bin = os.path.join(os.path.sep, 'usr', 'bin', 'mutter')
    self.fs.CreateFile(mutter_bin)
    os.chmod(mutter_bin, 0o755)

    def create_socket_file(*args, **kwargs):
      del args  # unused.
      env = kwargs.get('env', {})
      runtime_dir = env.get('XDG_RUNTIME_DIR')
      if runtime_dir:
        self.fs.CreateFile(os.path.join(runtime_dir, 'wayland-3'))
      return mock_proc

    mock_popen.side_effect = create_socket_file

    server = wayland_server.WaylandServer(
      worker_num=3, compositor_binary=mutter_bin
    )
    server.Start()

    self.assertTrue(server.is_running)
    self.assertEqual(server.socket_name, 'wayland-3')
    self.assertEqual(os.environ['WAYLAND_DISPLAY'], 'wayland-3')
    runtime_dir = os.environ['XDG_RUNTIME_DIR']
    self.assertTrue(os.path.exists(runtime_dir))

    server.Stop()
    self.assertFalse(server.is_running)
    mock_proc.terminate.assert_called_once()
    self.assertFalse(os.path.exists(runtime_dir))

  @mock.patch('subprocess.Popen')
  def testStartMutterPrematureExit(self, mock_popen):
    mock_proc = mock.MagicMock()
    mock_proc.poll.return_value = 1
    mock_proc.returncode = 1
    mock_popen.return_value = mock_proc

    mutter_bin = os.path.join(os.path.sep, 'usr', 'bin', 'mutter')
    self.fs.CreateFile(mutter_bin)
    os.chmod(mutter_bin, 0o755)

    server = wayland_server.WaylandServer(
      worker_num=1, compositor_binary=mutter_bin
    )

    with self.assertRaises(RuntimeError) as cm:
      server.Start()
    self.assertIn('Mutter failed to start with exit code 1', str(cm.exception))


if __name__ == '__main__':
  unittest.main()
