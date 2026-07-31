# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for ios/web_view.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os

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


def _CheckNotFatalUntilAdoption(input_api, output_api):
  """Encourages base::NotFatalUntil in iOS files."""
  def FileFilter(affected_file):
    if affected_file.Action() == 'D':
      return False
    return input_api.FilterSourceFile(
        affected_file,
        files_to_check=[r'.*\.(cc|h|mm)$'],
        files_to_skip=input_api.DEFAULT_FILES_TO_SKIP)

  # Regex for standard CHECKs (excluding CHECK_DEREF)
  check_re = input_api.re.compile(
      r'\b(CHECK|CHECK_EQ|CHECK_NE|CHECK_LT|CHECK_LE|CHECK_GT|CHECK_GE|PCHECK|'
      r'NOTREACHED)\s*\('
  )
  nfu_re = input_api.re.compile(r'base::NotFatalUntil')

  nfu_problems = []

  for f in input_api.AffectedSourceFiles(FileFilter):
    # Use NewContents for multi-line context, but only scan files with changes.
    changed_lines = set(line_num for line_num, _ in f.ChangedContents())
    if not changed_lines:
      continue

    new_contents = f.NewContents()
    for line_num in changed_lines:
      # line_num is 1-indexed.
      line = new_contents[line_num - 1]

      # Strip comments to avoid false positives.
      clean_line = input_api.re.sub(r'//.*', '', line)

      # Flag standard CHECKs missing NotFatalUntil
      if check_re.search(clean_line):
        # Check if this line or the next few lines contain NotFatalUntil.
        # This handles multi-line macros.
        context = '\n'.join(new_contents[line_num - 1 : line_num + 3])
        if nfu_re.search(context):
          continue

        # Check if this is a "promotion" (removing NotFatalUntil from an
        # existing CHECK). We check if the old version of this line also
        # contained a CHECK with NotFatalUntil.
        is_promotion = False
        if f.OldContents():
          # Look for a CHECK with NotFatalUntil in the old contents within a
          # small window to account for line shifts.
          old_window_start = max(0, line_num - 3)
          old_window_end = line_num + 2
          old_context = '\n'.join(
              f.OldContents()[old_window_start:old_window_end])
          if check_re.search(old_context) and nfu_re.search(old_context):
            is_promotion = True

        if not is_promotion:
          nfu_problems.append(
              '%s:%d: %s' % (f.LocalPath(), line_num, line.strip())
          )

  if nfu_problems:
    return [output_api.PresubmitPromptWarning(
        'Consider using base::NotFatalUntil for new CHECKs in iOS code to '
        'allow safer rollout (see style guide: https://chromium.googlesource.'
        'com/chromium/src/+/HEAD/styleguide/c++/checks.md#notfataluntil).\n'
        'Affected lines:\n' + '\n'.join(nfu_problems)
    )]
  return []


def _CheckDiscourageCheckDeref(input_api, output_api):
  """Discourages CHECK_DEREF in iOS files."""
  def FileFilter(affected_file):
    if affected_file.Action() == 'D':
      return False
    return input_api.FilterSourceFile(
        affected_file,
        files_to_check=[r'.*\.(cc|h|mm)$'],
        files_to_skip=input_api.DEFAULT_FILES_TO_SKIP)

  # Regex for CHECK_DEREF specifically
  check_deref_re = input_api.re.compile(r'\bCHECK_DEREF\s*\(')

  deref_problems = []

  for f in input_api.AffectedSourceFiles(FileFilter):
    for line_num, line in f.ChangedContents():
      # Strip comments to avoid false positives.
      clean_line = input_api.re.sub(r'//.*', '', line)

      if check_deref_re.search(clean_line):
        deref_problems.append(
            '%s:%d: %s' % (f.LocalPath(), line_num, line.strip())
        )

  if deref_problems:
    return [output_api.PresubmitPromptWarning(
        'Avoid using CHECK_DEREF in iOS/CWV code. Its design is incompatible '
        'with non-fatal rollouts (continuing on null triggers a hardware '
        'crash). Instead, use a standard CHECK with a milestone or a manual '
        'pointer check followed by a safe fallback (e.g., an early return).\n'
        'Affected lines:\n' + '\n'.join(deref_problems)
    )]
  return []


def _CheckCommon(input_api, output_api):
  results = []
  results.extend(
      _CheckAbsolutePathInclusionInPublicHeaders(input_api, output_api))
  results.extend(_CheckNotFatalUntilAdoption(input_api, output_api))
  results.extend(_CheckDiscourageCheckDeref(input_api, output_api))
  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CheckCommon(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckCommon(input_api, output_api)
