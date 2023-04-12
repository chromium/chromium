# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enforces json format.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'
USE_PYTHON3 = True

_IGNORE_FREEZE_FOOTER = 'Ignore-Freeze'

# The time module's handling of timezones is abysmal, so the boundaries are
# precomputed in UNIX time
_FREEZE_START = 1671177600  # 2022/12/16 00:00 -0800
_FREEZE_END = 1672646400  # 2023/01/02 00:00 -0800


def CheckFreeze(input_api, output_api):
  if _FREEZE_START <= input_api.time.time() < _FREEZE_END:
    footers = input_api.change.GitFootersFromDescription()
    if _IGNORE_FREEZE_FOOTER not in footers:

      def convert(t):
        ts = input_api.time.localtime(t)
        return input_api.time.strftime('%Y/%m/%d %H:%M %z', ts)

      # Don't report errors when on the presubmit --all bot or when testing
      # with presubmit --files.
      if input_api.no_diffs:
        report_type = output_api.PresubmitPromptWarning
      else:
        report_type = output_api.PresubmitError
      return [
          report_type('There is a prod freeze in effect from {} until {},'
                      ' files in //testing/buildbot cannot be modified'.format(
                          convert(_FREEZE_START), convert(_FREEZE_END)))
      ]

  return []


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
                                               run_on_python2=False,
                                               run_on_python3=True,
                                               skip_shebang_check=True)
  return input_api.RunTests(tests)


def CheckManageJsonFiles(input_api, output_api):
  return input_api.RunTests([
      input_api.Command(
          name='manage JSON files',
          cmd=[input_api.python3_executable, 'manage.py', '--check'],
          kwargs={},
          message=output_api.PresubmitError),
  ])


# TODO(gbeaty) pinpoint runs builds against revisions that aren't tip-of-tree,
# so recipe side config can't be updated to refer to
# //infra/config/generated/testing/gn_isolate_map.pyl until all of the revisions
# that pinpoint will run against have that file. To workaround this, we'll copy
# the generated file to //testing/buildbot/gn_isiolate_map.pyl. Once pinpoint is
# only building revisions that contain
# //infra/config/generated/testing/gn_isolate_map.pyl, the recipe configs can be
# updated and we can remove this presubmit check,
# //testing/buildbot/gn_isiolate_map.pyl and
# //infra/config/scripts/sync-isolate-map.py.
def CheckGnIsolateMapPylSynced(input_api, output_api):
  if ('testing/buildbot/gn_isolate_map.pyl' in input_api.change.LocalPaths()
      and 'infra/config/generated/testing/gn_isolate_map.pyl'
      not in input_api.change.LocalPaths()):
    return [
        output_api.PresubmitError(
            '//testing/buildbot/gn_isolate_map.pyl should not be edited'
            ' manually, instead modify //infra/config/targets/targets.star,'
            ' run //infra/config/main.star and run'
            ' //infra/config/scripts/sync-isolate-map.py')
    ]
  return []
