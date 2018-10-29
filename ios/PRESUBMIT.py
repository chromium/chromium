# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for ios.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os

TODO_PATTERN = r'TO[D]O\(([^\)]*)\)'
CRBUG_PATTERN = r'crbug\.com/\d+$'
ARC_COMPILE_GUARD = [
    '#if !defined(__has_feature) || !__has_feature(objc_arc)',
    '#error "This file requires ARC support."',
    '#endif',
]

def IsSubListOf(needle, hay):
  """Returns whether there is a slice of |hay| equal to |needle|."""
  for i, line in enumerate(hay):
    if line == needle[0]:
      if needle == hay[i:i+len(needle)]:
        return True
  return False

def _CheckARCCompilationGuard(input_api, output_api):
  """ Checks whether new objc files have proper ARC compile guards."""
  files_without_headers = []
  for f in input_api.AffectedFiles():
    if f.Action() != 'A':
      continue

    _, ext = os.path.splitext(f.LocalPath())
    if ext not in ('.m', '.mm'):
      continue

    if not IsSubListOf(ARC_COMPILE_GUARD, f.NewContents()):
      files_without_headers.append(f.LocalPath())

  if not files_without_headers:
    return []

  plural_suffix = '' if len(files_without_headers) == 1 else 's'
  error_message = '\n'.join([
      'Found new Objective-C implementation file%(plural)s without compile'
      ' guard%(plural)s. Please use the following compile guard'
      ':' % {'plural': plural_suffix}
  ] + ARC_COMPILE_GUARD + files_without_headers) + '\n'

  return [output_api.PresubmitError(error_message)]


def _CheckBugInToDo(input_api, output_api):
  """ Checks whether TODOs in ios code are identified by a bug number."""
  todo_regex = input_api.re.compile(TODO_PATTERN)
  crbug_regex = input_api.re.compile(CRBUG_PATTERN)

  errors = []
  for f in input_api.AffectedFiles():
    for line_num, line in f.ChangedContents():
      todo_match = todo_regex.search(line)
      if not todo_match:
        continue
      crbug_match = crbug_regex.match(todo_match.group(1))
      if not crbug_match:
        errors.append('%s:%s' % (f.LocalPath(), line_num))
  if not errors:
    return []

  plural_suffix = '' if len(errors) == 1 else 's'
  error_message = '\n'.join([
      'Found TO''DO%(plural)s without bug number%(plural)s (expected format is '
      '\"TO''DO(crbug.com/######)\":' % {'plural': plural_suffix}
  ] + errors) + '\n'

  return [output_api.PresubmitError(error_message)]


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CheckBugInToDo(input_api, output_api))
  results.extend(_CheckARCCompilationGuard(input_api, output_api))
  return results
