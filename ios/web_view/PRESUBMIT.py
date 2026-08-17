# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for ios/web_view.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os
import sys

INCLUSION_PREFIXES = ('#import "', '#include "')


def _CheckAbsolutePathInclusionInPublicHeaders(input_api, output_api):
  """Checks if all affected headers under //ios/web_view/public only include
     the headers in the same directory by using relative path inclusions.

     Because only these headers will be exported to the client side code,
     and path above public/ will be changed, the clients will not find the
     headers that are not in that public directory, and relative path
     inclusions should be used.
  """
  error_items = []  # [(file_path, lineno, corrected_inclusion_path)]
  normpath = os.path.normpath

  public_dir = normpath('%s/public/' % input_api.PresubmitLocalPath())
  files_under_public_dir = list(filter(
    lambda f: normpath(f.AbsoluteLocalPath()).startswith(public_dir),
    input_api.change.AffectedFiles()))

  for f in files_under_public_dir:
    _, ext = os.path.splitext(f.LocalPath())
    if ext != '.h':
      continue

    for idx, line in enumerate(f.NewContents()):
      lineno = idx + 1
      if line.startswith(INCLUSION_PREFIXES) and '/' in line:
        error_items.append((f.AbsoluteLocalPath(),
                            lineno,
                            line[line.rfind('/')+1:-1]))  # :-1 to exclude "

  if len(error_items) == 0:
    return []

  plural_suffix = '' if len(error_items) == 1 else 's'
  error_message = '\n'.join([
      'Found header file%(plural)s with absolute path inclusion%(plural)s '
      'in //ios/web_view/public.\n'
      'You can only include header files in //ios/web_view/public (no '
      'subdirectory) using relative path inclusions in the following '
      'file%(plural)s:\n' % {'plural': plural_suffix}
  ])
  error_message += '\n'.join([
    '    %(file_path)s [line %(lineno)d]:\n'
    '        Do you mean "%(corrected)s"?' % {
      'file_path': i[0], 'lineno': i[1], 'corrected': i[2]
    } for i in error_items
  ]) + '\n'
  return [output_api.PresubmitError(error_message)]


def _CheckCommon(input_api, output_api):
  results = []
  results.extend(
      _CheckAbsolutePathInclusionInPublicHeaders(input_api, output_api))

  # Import shared iOS NFU linter
  original_sys_path = sys.path[:]
  try:
    sys.path.append(input_api.change.RepositoryRoot())
    from build.ios import presubmit_support
    results.extend(
      presubmit_support.CheckNotFatalUntilAdoption(input_api, output_api)
    )
    results.extend(
      presubmit_support.CheckDiscourageCheckDeref(input_api, output_api)
    )
  finally:
    sys.path = original_sys_path

  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CheckCommon(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckCommon(input_api, output_api)
