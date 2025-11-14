# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'

import os
import re
import difflib


def CheckBundlesOrder(input_api, output_api):
  """Checks if targets.bundle declarations are sorted by name."""
  bundles_file = os.path.join(input_api.PresubmitLocalPath(), 'bundles.star')
  if bundles_file not in input_api.AbsoluteLocalPaths():
    return []

  content = input_api.ReadFile(bundles_file)
  names = []
  for line in content.split('\n'):
    name_match = re.search(r' {4}name = "(.+)",', line)
    if name_match:
      names.append(name_match.group(1))

  # Return error when there is no "name" in bundles.star.
  if not names:
    return [output_api.PresubmitError(f'No "name" is found in {bundles_file}')]

  sorted_names = sorted(names)
  if names != sorted_names:
    diff = difflib.unified_diff(
        [n + '\n' for n in sorted_names],
        [n + '\n' for n in names],
        fromfile='bundles.star (sorted)',
        tofile='bundles.star',
        lineterm='\n',
    )
    error_message = (
        'targets.bundle name are not sorted.\n' + ''.join(diff) + '\n'
        'For googlers: please run /google/bin/releases/keep-sorted/keep-sorted '
        f'{bundles_file} to sort it locally.\n'
        'Or use the automatic fix provided by the Keep Sorted analyzer.')

    if input_api.is_committing:
      return [output_api.PresubmitError(error_message)]
    else:
      return [output_api.PresubmitPromptWarning(error_message)]
  return []


def CheckLucicfgLint(input_api, output_api):
  """Checks if infra/config/targets/*.star files have lint issue or not."""
  d = input_api.PresubmitLocalPath()
  return input_api.RunTests([
      input_api.Command(
          name=f'lucicfg lint {d}',
          cmd=['lucicfg', 'lint', d],
          kwargs={},
          message=output_api.PresubmitError,
      )
  ])
