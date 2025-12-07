# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility functions related to screenshot tests."""

from gpu_tests import common_typing as ct


def GetTrueDpr(tab: ct.Tab) -> float:
  """Gets the true DPR as reported by the device.

  This is generally only useful for informational purposes or if you
  can safely assume that there is no scaling applied.

  Args:
    tab: A Telemetry Tab to retrieve the DPR from.

  Returns:
    The true DPR as a float.
  """
  return tab.EvaluateJavaScript('window.devicePixelRatio')


def GetEffectiveDpr(tab: ct.Tab) -> float:
  """Gets the effective DPR of the tab.

  Takes any viewport scaling into account, e.g. zooming out to fit
  content onto the page will result in a smaller effective DPR.

  Args:
    tab: A Telemetry Tab to retrieve the DPR from.

  Returns:
    The effective DPR as a float.
  """
  return tab.EvaluateJavaScript(
      'window.devicePixelRatio * window.visualViewport.scale')
