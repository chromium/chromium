# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


def CheckTestingBuildbotSourceSideSpecs(input_api, output_api):
  return input_api.RunTests([
      input_api.Command(
          name='check source side specs',
          cmd=[
              input_api.python3_executable,
              '../../../../testing/buildbot/generate_buildbot_json.py',
              '--check', '--verbose'
          ],
          kwargs={},
          message=output_api.PresubmitError),
  ])


def CheckTestingBuildbotJsonFiles(input_api, output_api):
  return input_api.RunTests([
      input_api.Command(name='check JSON files',
                        cmd=[
                            input_api.python3_executable,
                            '../../../../testing/buildbot/check.py'
                        ],
                        kwargs={},
                        message=output_api.PresubmitError),
  ])
