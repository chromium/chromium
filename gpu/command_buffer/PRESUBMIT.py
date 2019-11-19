# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Enforces command buffer autogen matches script output.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

import os.path


def _IsGLES2CmdBufferFile(file):
  filename = os.path.basename(file.LocalPath())
  if filename in [
      'build_cmd_buffer_lib.py', 'build_gles2_cmd_buffer.py',
      'gles2_cmd_buffer_functions.txt', 'gl2.h', 'gl2ext.h', 'gl3.h', 'gl31.h',
      'gl2chromium.h', 'gl2extchromium.h'
  ]:
    return True

  return ((filename.startswith('gles2') or filename.startswith('context_state')
           or filename.startswith('client_context_state')) and
          filename.endswith('_autogen.h'))


def _IsRasterCmdBufferFile(file):
  filename = os.path.basename(file.LocalPath())
  if filename in [
      'build_cmd_buffer_lib.py', 'build_raster_cmd_buffer.py',
      'raster_cmd_buffer_functions.txt'
  ]:
    return True

  return filename.startswith('raster') and filename.endswith('_autogen.h')


def _IsWebGPUCmdBufferFile(file):
  filename = os.path.basename(file.LocalPath())
  if filename in [
      'build_cmd_buffer_lib.py', 'build_webgpu_cmd_buffer.py',
      'webgpu_cmd_buffer_functions.txt'
  ]:
    return True

  return filename.startswith('webgpu') and filename.endswith('_autogen.h')


def CommonChecks(input_api, output_api):
  gles2_cmd_buffer_files = input_api.AffectedFiles(
      file_filter=_IsGLES2CmdBufferFile)
  raster_cmd_buffer_files = input_api.AffectedFiles(
      file_filter=_IsRasterCmdBufferFile)
  webgpu_cmd_buffer_files = input_api.AffectedFiles(
      file_filter=_IsWebGPUCmdBufferFile)

  messages = []

  with input_api.temporary_directory() as temp_dir:
    commands = []
    if len(gles2_cmd_buffer_files) > 0:
      commands.append(
          input_api.Command(
              name='build_gles2_cmd_buffer',
              cmd=[
                  input_api.python_executable, 'build_gles2_cmd_buffer.py',
                  '--check', '--output-dir=' + temp_dir
              ],
              kwargs={},
              message=output_api.PresubmitError))
    if len(raster_cmd_buffer_files) > 0:
      commands.append(
          input_api.Command(
              name='build_raster_cmd_buffer',
              cmd=[
                  input_api.python_executable, 'build_raster_cmd_buffer.py',
                  '--check', '--output-dir=' + temp_dir
              ],
              kwargs={},
              message=output_api.PresubmitError))
    if len(webgpu_cmd_buffer_files) > 0:
      commands.append(
          input_api.Command(
              name='build_webgpu_cmd_buffer',
              cmd=[
                  input_api.python_executable, 'build_webgpu_cmd_buffer.py',
                  '--check', '--output-dir=' + temp_dir
              ],
              kwargs={},
              message=output_api.PresubmitError))
    if len(commands) > 0:
      messages.extend(input_api.RunTests(commands))

  return messages


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
