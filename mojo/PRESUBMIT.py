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

def CheckChange(input_api, output_api):
  # Additional python module paths (we're in src/mojo/); not everyone needs
  # them, but it's easiest to add them to everyone's path.
  # For ply and jinja2:
  third_party_path = os.path.join(
      input_api.PresubmitLocalPath(), "..", "third_party")
  # For the bindings generator:
  mojo_public_bindings_pylib_path = os.path.join(
      input_api.PresubmitLocalPath(), "public", "tools", "bindings", "pylib")
  # For the python bindings:
  mojo_python_bindings_path = os.path.join(
      input_api.PresubmitLocalPath(), "public", "python")
  # TODO(vtl): Don't lint these files until the (many) problems are fixed
  # (possibly by deleting/rewriting some files).
  files_to_skip = input_api.DEFAULT_FILES_TO_SKIP + \
      (r".*\bpublic[\\\/]tools[\\\/]bindings[\\\/]pylib[\\\/]mojom[\\\/]"
           r"generate[\\\/].+\.py$",
       r".*\bpublic[\\\/]tools[\\\/]bindings[\\\/]checks[\\\/].+\.py$",
       r".*\bpublic[\\\/]tools[\\\/]bindings[\\\/]generators[\\\/].+\.py$",
       r".*\bspy[\\\/]ui[\\\/].+\.py$",
       r".*\btools[\\\/]pylib[\\\/]transitive_hash\.py$",
       r".*\btools[\\\/]test_runner\.py$")

  results = []
  pylint_extra_paths = [
      third_party_path,
      mojo_public_bindings_pylib_path,
      mojo_python_bindings_path,
  ]
  results += input_api.canned_checks.RunPylint(
      input_api, output_api, extra_paths_list=pylint_extra_paths,
      files_to_skip=files_to_skip)
  return results

def CheckGoldenFilesUpToDate(input_api, output_api):
  generate_script = os.path.join(input_api.PresubmitLocalPath(),
                                 'golden/generate.py')
  generated_dir = os.path.join(input_api.PresubmitLocalPath(),
                               'golden/generated')
  with tempfile.TemporaryDirectory() as tmp_dir:
    subprocess.run(['python3', generate_script, '--output-dir', tmp_dir],
                   check=True)
    dcmp = dircmp(tmp_dir, generated_dir)
    if len(dcmp.diff_files) == 0:
      return []
    return [output_api.PresubmitError(
      'Bindings generated from mojo/golden/corpus differ from '
      'golden files in mojo/golden/generated. Please regenerate '
      'golden files by running: mojo/golden/generate.py',
      items=dcmp.diff_files)]
