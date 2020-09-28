# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enforces luci-milo.cfg consistency.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckLintLuciMilo(input_api, output_api):
  if ('infra/config/generated/luci-milo.cfg' in input_api.LocalPaths() or
      'infra/config/lint-luci-milo.py' in input_api.LocalPaths()):
    return input_api.RunTests([
        input_api.Command(
            name='lint-luci-milo',
            cmd=[input_api.python_executable, 'lint-luci-milo.py'],
            kwargs={},
            message=output_api.PresubmitError),
    ])
  return []

def CheckTestingBuildbot(input_api, output_api):
  if ('infra/config/generated/luci-milo.cfg' in input_api.LocalPaths() or
      'infra/config/generated/luci-milo-dev.cfg' in input_api.LocalPaths()):
    return input_api.RunTests([
        input_api.Command(
            name='testing/buildbot config checks',
            cmd=[input_api.python_executable, input_api.os_path.join(
                '..', '..', 'testing', 'buildbot',
                'generate_buildbot_json.py',),
                 '--check'],
            kwargs={},
            message=output_api.PresubmitError),
    ])
  return []

def CheckLucicfgGenOutputMain(input_api, output_api):
  return input_api.RunTests(input_api.canned_checks.CheckLucicfgGenOutput(
      input_api, output_api, 'main.star'))

def CheckLucicfgGenOutputDev(input_api, output_api):
  return input_api.RunTests(input_api.canned_checks.CheckLucicfgGenOutput(
      input_api, output_api, 'dev.star'))

def CheckChangedLUCIConfigs(input_api, output_api):
  return input_api.canned_checks.CheckChangedLUCIConfigs(
      input_api, output_api)
