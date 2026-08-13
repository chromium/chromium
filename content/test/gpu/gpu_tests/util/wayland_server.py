# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for running isolated Wayland display servers."""

import atexit
import logging
import os
import shutil
import subprocess
import tempfile
import time
from typing import Optional

_START_TIMEOUT_SECONDS = 10
_TERMINATION_TIMEOUT_SECONDS = 3
_KILL_TIMEOUT_SECONDS = 2
_DEFAULT_WIDTH = 1920
_DEFAULT_HEIGHT = 1080


class WaylandServer:
  """Manages an isolated headless Wayland compositor instance.

  This is typically needed for GPU testing on Wayland since occluded browser
  windows run into issues and running tests in parallel is liable to cause
  the multiple browsers to occlude each other. Running each browser in its own
  Wayland display works around this occlusion issue.
  """

  def __init__(
    self, worker_num: int = 1, compositor_binary: Optional[str] = None
  ):
    self._worker_num = worker_num
    self._compositor_binary = compositor_binary
    self._proc: Optional[subprocess.Popen] = None
    self._runtime_dir: Optional[str] = None
    self._socket_name: Optional[str] = None
    self._orig_env: dict[str, Optional[str]] = {}

  @property
  def is_running(self) -> bool:
    return self._proc is not None and self._proc.poll() is None

  @property
  def socket_name(self) -> Optional[str]:
    return self._socket_name

  @property
  def runtime_dir(self) -> Optional[str]:
    return self._runtime_dir

  def Start(self) -> None:
    """Starts an isolated Wayland compositor and updates the environment."""
    if self.is_running:
      return

    compositor_bin = self._FindCompositorBinary()
    if not compositor_bin:
      raise RuntimeError('No Wayland compositor binary (mutter) found.')

    self._runtime_dir = tempfile.mkdtemp(
      prefix=f'xdg_gpu_worker_{self._worker_num}_{os.getpid()}_'
    )
    os.chmod(self._runtime_dir, 0o700)

    self._orig_env['XDG_RUNTIME_DIR'] = os.environ.get('XDG_RUNTIME_DIR')
    self._orig_env['WAYLAND_DISPLAY'] = os.environ.get('WAYLAND_DISPLAY')

    self._StartMutter(compositor_bin)

    # This should typically be handled by whatever owns this server, but
    # attempt to clean ourselves up to be safe.
    atexit.register(self.Stop)

  def _FindCompositorBinary(self) -> Optional[str]:
    """Finds a usable Wayland compositor binary path.

    Currently, only mutter is supported since that is what is used by default
    on Ubuntu, which is what the Swarming bots use.
    """
    if self._compositor_binary and os.path.exists(self._compositor_binary):
      return self._compositor_binary

    # 1. Check build directory if set.
    # The environment variable should also be set by Telemetry if
    # --chromium-output-directory is passed in.
    build_dir = os.environ.get('CHROMIUM_OUTPUT_DIR')
    if build_dir:
      candidate = os.path.join(build_dir, 'mutter')
      if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate

    # 2. Check current working directory.
    name = './mutter'
    if os.path.isfile(name) and os.access(name, os.X_OK):
      return os.path.abspath(name)

    # 3. Check system PATH.
    path = shutil.which('mutter')
    if path:
      return path

    return None

  def _StartMutter(self, mutter_bin: str) -> None:
    """Starts a Mutter compositor instance in headless mode."""
    assert self._runtime_dir is not None
    self._socket_name = f'wayland-{self._worker_num}'

    env = os.environ.copy()
    env['XDG_RUNTIME_DIR'] = self._runtime_dir

    cmd = [
      mutter_bin,
      '--headless',
      f'--virtual-monitor={_DEFAULT_WIDTH}x{_DEFAULT_HEIGHT}',
      f'--wayland-display={self._socket_name}',
    ]

    # pylint: disable=consider-using-with
    self._proc = subprocess.Popen(
      cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )
    # pylint: enable=consider-using-with

    start_time = time.monotonic()
    ready = False
    socket_path = os.path.join(self._runtime_dir, self._socket_name)
    while time.monotonic() - start_time < _START_TIMEOUT_SECONDS:
      if self._proc.poll() is not None:
        raise RuntimeError(
          f'Mutter failed to start with exit code {self._proc.returncode}'
        )
      if os.path.exists(socket_path):
        ready = True
        break
      time.sleep(0.05)

    if not ready:
      self.Stop()
      raise RuntimeError(
        'Timed out waiting for Mutter Wayland display server to start.'
      )

    os.environ['XDG_RUNTIME_DIR'] = self._runtime_dir
    os.environ['WAYLAND_DISPLAY'] = self._socket_name
    logging.info(
      'Started isolated Mutter Wayland display %s in %s (worker %d)',
      self._socket_name,
      self._runtime_dir,
      self._worker_num,
    )

  def Stop(self) -> None:
    """Stops the compositor instance and cleans up runtime directories."""
    try:
      atexit.unregister(self.Stop)
    except Exception:  # pylint: disable=broad-except
      pass

    if self._proc:
      try:
        self._proc.terminate()
        self._proc.wait(timeout=_TERMINATION_TIMEOUT_SECONDS)
      except (subprocess.TimeoutExpired, OSError):
        try:
          self._proc.kill()
          self._proc.wait(timeout=_KILL_TIMEOUT_SECONDS)
        except OSError:
          pass
      self._proc = None

    if self._runtime_dir and os.path.exists(self._runtime_dir):
      shutil.rmtree(self._runtime_dir, ignore_errors=True)
      self._runtime_dir = None

    for k, v in self._orig_env.items():
      if v is None:
        os.environ.pop(k, None)
      else:
        os.environ[k] = v
    self._orig_env.clear()
    self._socket_name = None
