# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enforces Vulkan function pointer autogen matches script output.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

import os.path


def CommonChecks(input_api, output_api):
  generating_files = input_api.AffectedFiles(
      file_filter=lambda x: os.path.basename(x.LocalPath()) in [
          'generate_bindings.py'])
  generated_files = input_api.AffectedFiles(
      file_filter=lambda x: os.path.basename(x.LocalPath()) in [
          'vulkan_function_pointers.cc', 'vulkan_function_pointers.h'])


  messages = []

  if generated_files and not generating_files:
    long_text = 'Changed files:\n'
    for file in generated_files:
      long_text += file.LocalPath() + '\n'
      long_text += '\n'
      messages.append(output_api.PresubmitError(
          'Vulkan function pointer generated files changed but the generator '
          'did not.', long_text=long_text))

  with input_api.temporary_directory() as temp_dir:
    commands = []
    if generating_files:
      commands.append(input_api.Command(name='generate_bindings',
                                        cmd=[input_api.python_executable,
                                             'generate_bindings.py',
                                             '--check',
                                             '--output-dir=' + temp_dir],
                                        kwargs={},
                                        message=output_api.PresubmitError))
    if commands:
      messages.extend(input_api.RunTests(commands))

  return messages

def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
