# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Various helper functions related to working with websockets."""

import logging
import time

from gpu_tests import common_typing as ct
from gpu_tests.util import websocket_server as wss


def HandleWebsocketReceiveTimeoutError(tab: ct.Tab,
                                       start_time: float,
                                       additional_info: str | None = None
                                       ) -> None:
  """Helper function for when a message is not received on time.

  Args:
    tab: The Telemetry Tab that the test was run in.
    start_time: The time.time() value that the test was started at.
    additional_info: An optional string to output alongside common logging.
  """
  logging.error(
      'Timed out waiting for websocket message (%.3f seconds since test '
      'start), checking for hung renderer',
      time.time() - start_time)
  if additional_info:
    logging.error('Additional information provided by test: %s',
                  additional_info)
  # Telemetry has some code to automatically crash the renderer and GPU
  # processes if it thinks that the renderer is hung. So, execute some
  # trivial JavaScript now to hit that code if we got the timeout because of
  # a hung renderer. If we do detect a hung renderer, this will raise
  # another exception and prevent the following line about the renderer not
  # being hung from running.
  tab.action_runner.EvaluateJavaScript('let somevar = undefined;', timeout=5)
  logging.error('Timeout does *not* appear to be due to a hung renderer')


def HandlePrematureSocketClose(original_error: wss.ClientClosedConnectionError,
                               start_time: float,
                               additional_info: str | None = None) -> None:
  """Helper function for when a websocket connection is closed early.

  Args:
    original_error: The original error that was raised to signal a socket
      closure.
    start_time: The time.time() value that the test was started at.
    additional_info: An optional string to output alongside common logging.
  """
  elapsed = time.time() - start_time
  extra_info_str = ''
  if additional_info:
    extra_info_str = (
        f' Additional information provided by the test: {additional_info}')
  raise RuntimeError(
      f'Detected closed websocket ({elapsed:.3f} seconds since test start) - '
      f'likely caused by a renderer crash.{extra_info_str}') from original_error


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
