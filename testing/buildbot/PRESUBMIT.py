# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enforces json format.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'
USE_PYTHON3 = True


def CheckSourceSideSpecs(input_api, output_api):
  return input_api.RunTests([
      input_api.Command(name='check source side specs',
                        cmd=[
                            input_api.python3_executable,
                            'generate_buildbot_json.py', '--check', '--verbose'
                        ],
                        kwargs={},
                        message=output_api.PresubmitError),
  ])


def CheckTests(input_api, output_api):
  glob = input_api.os_path.join(input_api.PresubmitLocalPath(), '*test.py')
  tests = input_api.canned_checks.GetUnitTests(input_api,
                                               output_api,
                                               input_api.glob(glob),
                                               run_on_python2=False)
  return input_api.RunTests(tests)


def CheckManageJsonFiles(input_api, output_api):
  return input_api.RunTests([
      input_api.Command(
          name='manage JSON files',
          cmd=[input_api.python3_executable, 'manage.py', '--check'],
          kwargs={},
          message=output_api.PresubmitError),
  ])
