# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for media/test/.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

def _CheckTestDataReadmeUpdated(input_api, output_api):
  """
  Checks to make sure the README.md file is updated when changing test files.
  """
  test_data_dir = input_api.os_path.join('media', 'test', 'data')
  readme_path = input_api.os_path.join('media', 'test', 'data', 'README.md')
  test_files = []
  readme_updated = False
  errors = []
  for f in input_api.AffectedFiles():
    local_path = f.LocalPath()
    if input_api.os_path.dirname(local_path) == test_data_dir:
      test_files.append(f)
      if local_path == readme_path:
        readme_updated = True
        break
  if test_files and not readme_updated:
    errors.append(output_api.PresubmitPromptWarning(
        'When updating files in ' + test_data_dir + ', please also update '
        + readme_path + ':', test_files))
  return errors

def CheckChangeOnUpload(input_api, output_api):
  return _CheckTestDataReadmeUpdated(input_api, output_api)
