# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enforces json format.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckFreeze(input_api, output_api):
  return input_api.canned_checks.CheckInfraFreeze(
      input_api, output_api, files_to_exclude=['.+/filters/.+'])


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
  for f in input_api.AffectedFiles():
    # If the only files changed here match //testing/buildbot/*.(pyl|json),
    # then we can assume the unit tests are unaffected.
    if (len(f.LocalPath().split(input_api.os_path.sep)) != 3
        or not f.LocalPath().endswith(('.json', '.pyl'))):
      break
  else:
    return []
  glob = input_api.os_path.join(input_api.PresubmitLocalPath(), '*test.py')
  tests = input_api.canned_checks.GetUnitTests(input_api, output_api,
                                               input_api.glob(glob))
  return input_api.RunTests(tests)


def CheckJsonFiles(input_api, output_api):
  return input_api.RunTests([
      input_api.Command(name='check JSON files',
                        cmd=[input_api.python3_executable, 'check.py'],
                        kwargs={},
                        message=output_api.PresubmitError),
  ])


def CheckPylFilesSynced(input_api, output_api):
  return input_api.RunTests([
      input_api.Command(
          name='check-pyl-files-synced',
          cmd=[
              input_api.python3_executable,
              '../../infra/config/scripts/sync-pyl-files.py',
              '--check',
          ],
          kwargs={},
          message=output_api.PresubmitError,
      ),
  ])
