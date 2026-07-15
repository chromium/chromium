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
    script_path = input_api.os_path.join(flag_tools_dir, 'lint_flags.py')
    cmd = [input_api.python3_executable, script_path]

    # Use Command API so that the check can run concurrently when --parallel
    # is used.
    return input_api.RunTests([
        input_api.Command(
            name='CheckForOrphanedFlagMetadata',
            cmd=cmd,
            kwargs={'cwd': flag_tools_dir},
            message=output_api.PresubmitError
        )
    ])
