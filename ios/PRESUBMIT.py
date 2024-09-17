# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for ios.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os

NULLABILITY_PATTERN = r'(nonnull|nullable|_Nullable|_Nonnull)'
TODO_PATTERN = r'TO[D]O\(([^\)]*)\)'
BUG_PATTERN = r'^(crbug\.com|b)/\d+$'
DEPRECATED_BUG_PATTERN = r'^b/\d+$'
INCLUDE_PATTERN = r'^#include'
PIPE_IN_COMMENT_PATTERN = r'//.*[^|]\|(?!\|)'
IOS_PACKAGE_PATTERN = r'^ios'
BOXED_BOOL_PATTERN = r'@\((YES|NO)\)'

def IsSubListOf(needle, hay):
    """Returns whether there is a slice of |hay| equal to |needle|."""
    for i, line in enumerate(hay):
        if line == needle[0]:
            if needle == hay[i:i + len(needle)]:
                return True
    return False


def _CheckNullabilityAnnotations(input_api, output_api):
    """ Checks whether there are nullability annotations in ios code.

    They are accepted in ios/web_view/public since it tries to mimic
    the platform library but not anywhere else.
    """
    nullability_regex = input_api.re.compile(NULLABILITY_PATTERN)

    errors = []
    for f in input_api.AffectedFiles():
        if f.LocalPath().startswith('ios/web_view/public/'):
            # ios/web_view/public tries to mimic an existing API that
            # might have nullability in it and that is acceptable.
            continue
        for line_num, line in f.ChangedContents():
            if nullability_regex.search(line):
                errors.append('%s:%s' % (f.LocalPath(), line_num))
    if not errors:
        return []

    plural_suffix = '' if len(errors) == 1 else 's'
    warning_message = ('Found Nullability annotation%(plural)s. '
                     'Prefer DCHECKs in ios code to check for nullness:' % {
                         'plural': plural_suffix
                     })

    return [output_api.PresubmitPromptWarning(warning_message, items=errors)]


def _CheckBugInToDo(input_api, output_api):
    """ Checks whether TODOs in ios code are identified by a bug number."""
    errors = []
    warnings = []
    for f in input_api.AffectedFiles():
        for line_num, line in f.ChangedContents():
            if _HasToDoWithNoBug(input_api, line):
                errors.append('%s:%s' % (f.LocalPath(), line_num))
            if _HasToDoWithDeprecatedBug(input_api, line):
                warnings.append('%s:%s' % (f.LocalPath(), line_num))
    if not errors and not warnings:
        return []

    output = []
    if errors:
      singular_article = 'a ' if len(errors) == 1 else ''
      plural_suffix = '' if len(errors) == 1 else 's'
      error_message = '\n'.join([
          'Found TO'
          'DO%(plural)s without %(a)sbug number%(plural)s (expected format '
          'is \"TO'
          'DO(crbug.com/######)\"):' % {
              'plural': plural_suffix,
              'a' : singular_article
          }
      ] + errors) + '\n'
      output.append(output_api.PresubmitError(error_message))

    if warnings:
      singular_article = 'a ' if len(warnings) == 1 else ''
      plural_suffix = '' if len(warnings) == 1 else 's'
      warning_message = '\n'.join([
          'Found TO'
          'DO%(plural)s with %(a)sdeprecated bug link%(plural)s (found '
          '"b/#####\", expected format is \"crbug.com/######"):' % {
              'plural': plural_suffix,
              'a' : singular_article
          }
      ] + warnings) + '\n'
      output.append(output_api.PresubmitPromptWarning(warning_message))

    return output


def _CheckHasNoIncludeDirectives(input_api, output_api):
    """ Checks that #include preprocessor directives are not present."""
    errors = []
    for f in input_api.AffectedFiles():
        if not _IsInIosPackage(input_api, f.LocalPath()):
            continue
        _, ext = os.path.splitext(f.LocalPath())
        if ext != '.mm':
            continue
        for line_num, line in f.ChangedContents():
            if _HasIncludeDirective(input_api, line):
                errors.append('%s:%s' % (f.LocalPath(), line_num))
    if not errors:
        return []

    singular_plural = 'it' if len(errors) == 1 else 'them'
    plural_suffix = '' if len(errors) == 1 else 's'
    error_message = '\n'.join([
        'Found usage of `#include` preprocessor directive%(plural)s! Please, '
        'replace %(singular_plural)s with `#import` preprocessor '
        'directive%(plural)s instead. '
        'Consider replacing all existing `#include` with `#import` (if any) in '
        'this file for the code clean up. See '
        'https://chromium.googlesource.com/chromium/src.git/+/refs/heads/main'
        '/styleguide/objective-c/objective-c.md'
        '#import-and-include-in-the-directory for more details. '
        '\n\nAffected file%(plural)s:' % {
            'plural': plural_suffix,
            'singular_plural': singular_plural
        }
    ] + errors) + '\n'

    return [output_api.PresubmitError(error_message)]


def _CheckHasNoPipeInComment(input_api, output_api):
    """ Checks that comments don't contain pipes."""
    pipe_regex = input_api.re.compile(PIPE_IN_COMMENT_PATTERN)

    errors = []
    for f in input_api.AffectedFiles():
        if not _IsInIosPackage(input_api, f.LocalPath()):
            continue
        for line_num, line in f.ChangedContents():
            if pipe_regex.search(line):
                errors.append('%s:%s' % (f.LocalPath(), line_num))
    if not errors:
        return []
    warning_message = '\n'.join([
        'Please use backticks "`" instead of pipes "|" if you need to quote'
        ' variable names and symbols in comments.\n'
        'Found potential uses of pipes in:'
    ] + errors) + '\n'

    return [output_api.PresubmitPromptWarning(warning_message)]

def _CheckHasNoChromeBrowserStateForwardDeclaration(input_api, output_api):
    """ Checks that header files don't forward-declare ChromeBrowserState."""
    errors = []
    for f in input_api.AffectedFiles():
        for line_num, line in f.ChangedContents():
            if line == 'class ChromeBrowserState;':
                errors.append('%s:%s' % (f.LocalPath(), line_num))
    if not errors:
        return []

    plural_suffix = '' if len(errors) == 1 else 's'
    error_message = '\n'.join([
         'Found forward-declaration%(plural)s of ChromeBrowserState. Please'
         ' instead import this header:'
         ' ios/chrome/browser/shared/model/profile/profile_ios_forward.h'
         '\n\nAffected file%(plural)s:' % {
            'plural': plural_suffix,
          }
    ] + errors) + '\n'

    return [output_api.PresubmitError(error_message)]

def _CheckCanImproveTestUsingExpectNSEQ(input_api, output_api):
    """ Checks that test files use EXPECT_NSEQ when possible."""
    errors = []
    # Substrings that should not be used together with EXPECT_TRUE or
    # EXPECT_FALSE in tests.
    wrong_patterns = ["isEqualToString:", "isEqualToData:", "isEqualToArray:"]
    for f in input_api.AffectedFiles():
        if not '_unittest.' in f.LocalPath():
          continue
        for line_num, line in f.ChangedContents():
            if line.startswith(("EXPECT_TRUE", "EXPECT_FALSE")):
              # Condition is in one line.
              if any(x in line for x in wrong_patterns):
                errors.append('%s:%s' % (f.LocalPath(), line_num))
              # Condition is split on multiple lines.
              elif not line.endswith(";"):
                # Check this is not the last line.
                if line_num < len(f.NewContents()):
                  next_line = f.NewContents()[line_num]
                  if any(x in next_line for x in wrong_patterns):
                    errors.append('%s:%s' % (f.LocalPath(), line_num))

    if not errors:
        return []

    plural_suffix = '' if len(errors) == 1 else 's'
    warning_message = '\n'.join([
         'Found possible improvement in unittest. Prefer using'
         ' EXPECT_NSEQ() or EXPECT_NSNE() when possible.'
         '\n\nAffected file%(plural)s:' % {
            'plural': plural_suffix,
          }
    ] + errors) + '\n'

    return [output_api.PresubmitPromptWarning(warning_message)]

def _IsInIosPackage(input_api, path):
    """ Returns True if path is within ios package"""
    ios_package_regex = input_api.re.compile(IOS_PACKAGE_PATTERN)

    return ios_package_regex.search(path)


def _HasIncludeDirective(input_api, line):
    """ Returns True if #include is found in the line"""
    include_regex = input_api.re.compile(INCLUDE_PATTERN)

    return include_regex.search(line)


def _HasToDoWithNoBug(input_api, line):
    """ Returns True if TODO is not identified by a bug number."""
    todo_regex = input_api.re.compile(TODO_PATTERN)
    bug_regex = input_api.re.compile(BUG_PATTERN)

    todo_match = todo_regex.search(line)
    if not todo_match:
        return False

    return not bug_regex.match(todo_match.group(1))

def _HasToDoWithDeprecatedBug(input_api, line):
    """ Returns True if TODO is identified by a deprecated bug number format."""
    todo_regex = input_api.re.compile(TODO_PATTERN)
    deprecated_bug_regex = input_api.re.compile(DEPRECATED_BUG_PATTERN)

    todo_match = todo_regex.search(line)
    if not todo_match:
        return False
    return deprecated_bug_regex.match(todo_match.group(1))

def _CheckHasNoBoxedBOOL(input_api, output_api):
    """ Checks that there are no @(YES) or @(NO)."""
    boxed_BOOL_regex = input_api.re.compile(BOXED_BOOL_PATTERN)

    errors = []
    for f in input_api.AffectedFiles():
        for line_num, line in f.ChangedContents():
            if boxed_BOOL_regex.search(line):
                errors.append('%s:%s' % (f.LocalPath(), line_num))
    if not errors:
        return []

    plural_suffix = '' if len(errors) == 1 else 's'
    warning_message = ('Found boxed BOOL%(plural)s. '
                     'Prefer @YES or @NO in ios code:' % {
                         'plural': plural_suffix
                     })

    return [output_api.PresubmitPromptWarning(warning_message, items=errors)]

def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CheckBugInToDo(input_api, output_api))
    results.extend(_CheckNullabilityAnnotations(input_api, output_api))
    results.extend(_CheckHasNoIncludeDirectives(input_api, output_api))
    results.extend(_CheckHasNoPipeInComment(input_api, output_api))
    results.extend(_CheckHasNoBoxedBOOL(input_api, output_api))
    results.extend(_CheckHasNoChromeBrowserStateForwardDeclaration(input_api,
        output_api))
    results.extend(_CheckCanImproveTestUsingExpectNSEQ(input_api, output_api))
    return results
