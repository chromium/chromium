# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for ios/build/tools."""


def _RunBuildToolsUnitTests(input_api, output_api):
  """Runs iOS build tools unit tests."""
  if input_api.is_windows:
    return []
  files = ['.*_test.py$']
  return input_api.canned_checks.RunUnitTestsInDirectory(
    input_api, output_api, '.', files_to_check=files
  )


def CheckChange(input_api, output_api):
  results = []
  results.extend(_RunBuildToolsUnitTests(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)
