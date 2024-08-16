# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Various helper functions related to working with websockets."""

import logging
import time

from gpu_tests import common_typing as ct
from gpu_tests.util import websocket_server as wss


def HandleWebsocketReceiveTimeoutError(tab: ct.Tab, start_time: float) -> None:
  """Helper function for when a message is not received on time."""
  logging.error(
      'Timed out waiting for websocket message (%.3f seconds since test '
      'start), checking for hung renderer',
      time.time() - start_time)
  # Telemetry has some code to automatically crash the renderer and GPU
  # processes if it thinks that the renderer is hung. So, execute some
  # trivial JavaScript now to hit that code if we got the timeout because of
  # a hung renderer. If we do detect a hung renderer, this will raise
  # another exception and prevent the following line about the renderer not
  # being hung from running.
  tab.action_runner.EvaluateJavaScript('let somevar = undefined;', timeout=5)
  logging.error('Timeout does *not* appear to be due to a hung renderer')


def HandlePrematureSocketClose(original_error: wss.ClientClosedConnectionError,
                               start_time: float) -> None:
  raise RuntimeError(
      'Detected closed websocket (%.3f seconds since test start) - likely '
      'caused by a renderer crash' %
      (time.time() - start_time)) from original_error


def GetScaledConnectionTimeout(num_jobs: int) -> float:
  """Gets a scaled Websocket setup timeout based on number of test jobs.

  Args:
    num_jobs: How many parallel test jobs are being run.

  Returns:
    The scaled timeout to pass to WaitForConnection that accounts for slowness
    caused by multiple test jobs.
  """
  # Target a 2x multiplier when running 4 jobs.
  multiplier = 1 + (num_jobs - 1) / 3.0
  return multiplier * wss.WEBSOCKET_SETUP_TIMEOUT_SECONDS
