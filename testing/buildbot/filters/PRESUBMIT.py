# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for //testing/buildbot/filters.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os
import re


def _CheckFilterFileFormat(input_api, output_api):
  """This ensures all modified filter files are free of common syntax errors.

  See the following for the correct syntax of these files:
  https://chromium.googlesource.com/chromium/src/+/master/testing/buildbot/filters/README.md#file-syntax
  As well as:
  https://bit.ly/chromium-test-list-format
  """
  errors = []
  warnings = []
  for f in input_api.AffectedFiles():
    filename = os.path.basename(f.LocalPath())
    if not filename.endswith('.filter'):
      # Non-filter files. Ignore these.
      continue

    inclusions = 0
    exclusions = 0
    for line_num, line in enumerate(f.NewContents()):
      # Implicitly allow for trailing (but not leading) whitespace.
      line = line.rstrip()
      if not line:
        # Empty line. Ignore these.
        continue
      if line.startswith('#'):
        # A comment. Ignore these.
        continue
      if line.find('#') >= 0:
        errors.append(
            '%s:%d "#" is not a valid method separator.  Use ".": "%s"' % (
                filename, line_num, line))
        continue
      if line.startswith('//') or line.startswith('/*'):
        errors.append(
            '%s:%d Not a valid comment syntax. Use "#" instead: "%s"' % (
                filename, line_num, line))
        continue
      if not re.match(r'^\S+$', line):
        errors.append(
            '%s:%d Line must not contain whitespace: "%s"' % (
                filename, line_num, line))
        continue
      if line[0] == '-':
        exclusions += 1
      else:
        inclusions += 1

    # If we have a mix of exclusions and inclusions, print a warning with a
    # Y/N prompt to the author. Though this is a valid syntax, it's possible
    # that such a combination will lead to a situation where zero tests are run.
    if exclusions and inclusions:
      warnings.append(
          '%s: Contains both inclusions (%d) and exclusions (%d). This may '
          'result in no tests running. Are you sure this is correct?' % (
              filename, inclusions, exclusions))

  res = []
  if errors:
    res.append(output_api.PresubmitError(
        'Filter files do not follow the correct format:',
        long_text='\n'.join(errors)))
  if warnings:
    res.append(output_api.PresubmitPromptWarning(
        'Filter files may be incorrect:\n%s' % '\n'.join(warnings)))
  return res


def CommonChecks(input_api, output_api):
  return _CheckFilterFileFormat(input_api, output_api)


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
