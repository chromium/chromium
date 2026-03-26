# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for //ios/chrome/browser/flags.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckForOrphanedFlagMetadata(input_api, output_api):
    about_flags_path = input_api.os_path.join('ios', 'chrome', 'browser',
                                              'flags', 'about_flags.mm')
    if not any(f.LocalPath() == about_flags_path
               for f in input_api.AffectedFiles(include_deletes=False)):
      # Keep presubmit fast when `about_flags.mm` isn't modified.
      return []

    flag_tools_dir = input_api.os_path.join(input_api.change.RepositoryRoot(),
                                            'tools', 'flags')
    cmd = [input_api.python3_executable,
           input_api.os_path.join(flag_tools_dir, 'lint_flags.py')]
    try:
      # Run from `//tools/flags/` to give access to the `flags_utils` module.
      input_api.subprocess.check_call(cmd,
                                      cwd=flag_tools_dir,
                                      stdout=input_api.subprocess.PIPE)
      return []
    except input_api.subprocess.CalledProcessError as error:
      result = input_api.json.loads(error.stdout)
      # Output a hard error to block new orphans from landing.
      return [
        output_api.PresubmitError(
            message=(
                '`//chrome/browser/flag-metadata.json` appears to contain '
                'entries not used in `about_flags.cc` or `about_flags.mm`.'),
            items=result['unused_flags'])
      ]
