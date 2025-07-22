# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enforces luci-milo.cfg consistency.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckFreeze(input_api, output_api):
  return input_api.canned_checks.CheckInfraFreeze(input_api, output_api)


def CheckTests(input_api, output_api):
  glob = input_api.os_path.join(input_api.PresubmitLocalPath(), '*_test.py')
  tests = input_api.canned_checks.GetUnitTests(input_api, output_api,
                                               input_api.glob(glob))
  return input_api.RunTests(tests)


def CheckLintLuciMilo(input_api, output_api):
  if ('infra/config/generated/luci/luci-milo.cfg' in input_api.LocalPaths()
      or 'infra/config/lint-luci-milo.py' in input_api.LocalPaths()):
    return input_api.RunTests([
        input_api.Command(
            name='lint-luci-milo',
            cmd=[input_api.python3_executable, 'lint-luci-milo.py'],
            kwargs={},
            message=output_api.PresubmitError),
    ])
  return []

def CheckTestingBuildbot(input_api, output_api):
  if ('infra/config/generated/luci/luci-milo.cfg' in input_api.LocalPaths()
      or 'infra/config/generated/luci/luci-milo-dev.cfg'
      in input_api.LocalPaths()):
    return input_api.RunTests([
        input_api.Command(name='testing/buildbot config checks',
                          cmd=[
                              input_api.python3_executable,
                              input_api.os_path.join(
                                  '..',
                                  '..',
                                  'testing',
                                  'buildbot',
                                  'generate_buildbot_json.py',
                              ), '--check'
                          ],
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


def CheckPylFilesSynced(input_api, output_api):
  return input_api.RunTests([
      input_api.Command(
          name='check-pyl-files-synced',
          cmd=[
              input_api.python3_executable,
              'scripts/sync-pyl-files.py',
              '--check',
          ],
          kwargs={},
          message=output_api.PresubmitError,
      ),
  ])


# Footer indicating a CL that is trying to address an outage by some mechanism
# other than those in infra/config/outages
_OUTAGE_ACTION_FOOTER = 'Infra-Config-Outage-Action'
# Footer acknowledging that an outages configuration is in effect when making an
# unrelated change
_IGNORE_OUTAGE_FOOTER = 'Infra-Config-Ignore-Outage'

def CheckOutagesConfigOnCommit(input_api, output_api):
  outages_pyl = input_api.os_path.join(
      input_api.PresubmitLocalPath(), 'generated/outages.pyl')
  with open(outages_pyl, encoding='utf-8') as f:
    outages_config = input_api.ast.literal_eval(f.read())

  if not outages_config:
    footers = input_api.change.GitFootersFromDescription()
    return [
        output_api.PresubmitError(
            'There is no outages configuration in effect, '
            'please remove the {} footer from your CL description.'
            .format(footer))
        for footer in (_OUTAGE_ACTION_FOOTER, _IGNORE_OUTAGE_FOOTER)
        if footer in footers
    ]

  # Any of the config files under infra/config/outages
  outages_config_files = set()
  # Any of the config files under infra/config/generated
  generated_config_files = set()
  # Any config files that are not under infra/config/outages or
  # infra/config/generated
  config_files = set()
  for p in input_api.LocalPaths():
    if p in ('README.md', 'OWNERS'):
      continue
    if p.startswith('infra/config/outages/'):
      outages_config_files.add(p)
      continue
    if p.startswith('infra/config/generated/'):
      generated_config_files.add(p)
      continue
    config_files.add(p)

  # If the only changes to non-generated config fies were the outages files,
  # assume the change was addressing an outage and that no additional mechanism
  # needs to be added
  if outages_config_files and not config_files:
    # REVIEWER: Should we prevent the footers from being here in this case?
    return []

  # If any non-generated, non-outages files were modified or if the generated
  # config files were modified without any config files being modified (lucicfg
  # change, etc.) then make sure the user knows that when the outages
  # configuration is disabled, the generated configuration may change
  if config_files or generated_config_files:
    footers = input_api.change.GitFootersFromDescription()

    has_action_footer = _OUTAGE_ACTION_FOOTER in footers
    has_ignore_footer = _IGNORE_OUTAGE_FOOTER in footers

    if has_action_footer and has_ignore_footer:
      return [
          output_api.PresubmitError(
              'Only one of {} or {} should be present in your CL description'
              .format(_OUTAGE_ACTION_FOOTER, _IGNORE_OUTAGE_FOOTER)),
      ]

    if not has_action_footer and not has_ignore_footer:
      outages_config_lines = ['{}: {}'.format(k, v)
                              for k, v in sorted(outages_config.items())]
      return [
          output_api.PresubmitError('\n'.join([
              'The following outages configuration is in effect:\n  {}'.format(
                  '\n  '.join(outages_config_lines)),
              ('The effect of your change may not be visible '
               'in the generated configuration.'),
              ('If your change is addressing the outage, '
               'please add the footer {} with a link for the outage.'
               ).format(_OUTAGE_ACTION_FOOTER),
              ('If your change is not addressing the outage '
               'but you still wish to land it, please add the footer '
               '{} with a reason.').format(_IGNORE_OUTAGE_FOOTER),
              ('For more information on outages configuration, '
               'see https://chromium.googlesource.com/chromium/src/+/HEAD/infra/config/outages'
               ),
          ])),
      ]

  return []
