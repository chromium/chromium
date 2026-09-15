# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


def CheckPythonTests(input_api, output_api):
  tools_dir = input_api.os_path.join(input_api.PresubmitLocalPath(), 'tools')
  return input_api.RunTests(
      input_api.canned_checks.GetUnitTestsInDirectory(
          input_api,
          output_api,
          tools_dir,
          files_to_check=[r'.+_(?:unit)?test\.py$']))
