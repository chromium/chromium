# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for mojo

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

from filecmp import dircmp
import os.path
import subprocess
import tempfile

PRESUBMIT_VERSION = '2.0.0'


def CheckPatchFormatted(input_api, output_api):
  return input_api.canned_checks.CheckPatchFormatted(
    input_api,
    output_api,
    result_factory=output_api.PresubmitError,
    bypass_warnings=False,
  )


def CheckRuff(input_api, output_api):
  return input_api.RunTests(
    input_api.canned_checks.GetRuff(input_api, output_api)
  )


def CheckGoldenFilesUpToDate(input_api, output_api):
  if not input_api.HasAffectedFiles(path=['golden', 'public/tools']):
    return []
  generate_script = os.path.join(
    input_api.PresubmitLocalPath(), 'golden/generate.py'
  )
  generated_dir = os.path.join(
    input_api.PresubmitLocalPath(), 'golden/generated'
  )
  with tempfile.TemporaryDirectory() as tmp_dir:
    subprocess.run(
      ['python3', generate_script, '--output-dir', tmp_dir], check=True
    )
    diff_files = []
    for _, dcmp in dircmp(tmp_dir, generated_dir).subdirs.items():
      diff_files += dcmp.diff_files
    if len(diff_files) == 0:
      return []
    return [
      output_api.PresubmitError(
        'Bindings generated from mojo/golden/corpus differ from '
        'golden files in mojo/golden/generated. Please regenerate '
        'golden files by running: mojo/golden/generate.py',
        items=diff_files,
      )
    ]
