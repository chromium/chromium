# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A PRESUBMIT script to prevent direct edits to cached documentation."""

PRESUBMIT_VERSION = '2.0.0'

def CheckChangeOnUpload(input_api, output_api):
  """
  Checks that no cached documents in this directory are manually edited.
  """
  results = []
  protected_dir = input_api.PresubmitLocalPath()

  # A list of files within this directory that are allowed to be edited.
  allowed_to_edit = [
      input_api.os_path.join(protected_dir, 'readme.md'),
      input_api.os_path.join(protected_dir, 'refresh_docs.py'),
      input_api.os_path.join(protected_dir, 'PRESUBMIT.py'),
  ]

  for f in input_api.AffectedFiles():
    # Check if the file is inside this directory, but is not one of the
    # allowed files.
    if (f.LocalPath().startswith(protected_dir) and
        f.LocalPath() not in allowed_to_edit):
      results.append(
          output_api.PresubmitError(
              f'Direct edits to cached documents are not allowed.\n'
              f'Found modification in: {f.LocalPath()}\n'
              f'To update these docs, please modify and run the '
              f'refresh_docs.py script.'))

  return results
