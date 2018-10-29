# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enforces json format.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""


def CommonChecks(input_api, output_api):
  commands = [
    input_api.Command(
      name='generate_buildbot_json', cmd=[
        input_api.python_executable, 'generate_buildbot_json.py', '--check',
        '--verbose'],
      kwargs={}, message=output_api.PresubmitError),

    input_api.Command(
      name='generate_buildbot_json_unittest', cmd=[
        input_api.python_executable, 'generate_buildbot_json_unittest.py'],
      kwargs={}, message=output_api.PresubmitError),

    input_api.Command(
      name='generate_buildbot_json_coveragetest', cmd=[
        input_api.python_executable, 'generate_buildbot_json_coveragetest.py'],
      kwargs={}, message=output_api.PresubmitError),

    input_api.Command(
      name='manage', cmd=[
        input_api.python_executable, 'manage.py', '--check'],
      kwargs={}, message=output_api.PresubmitError),
  ]
  messages = []

  messages.extend(input_api.RunTests(commands))
  return messages


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
