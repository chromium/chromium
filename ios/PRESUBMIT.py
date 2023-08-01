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
BUG_PATTERN = r'(crbug\.com|b)/\d+$'
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
    """ Checks whether there are nullability annotations in ios code."""
    nullability_regex = input_api.re.compile(NULLABILITY_PATTERN)

    errors = []
    for f in input_api.AffectedFiles():
        for line_num, line in f.ChangedContents():
            if nullability_regex.search(line):
                errors.append('%s:%s' % (f.LocalPath(), line_num))
    if not errors:
        return []

    plural_suffix = '' if len(errors) == 1 else 's'
    error_message = ('Found Nullability annotation%(plural)s. '
                     'Prefer DCHECKs in ios code to check for nullness:' % {
                         'plural': plural_suffix
                     })

    return [output_api.PresubmitPromptWarning(error_message, items=errors)]


def _CheckBugInToDo(input_api, output_api):
    """ Checks whether TODOs in ios code are identified by a bug number."""
    errors = []
    for f in input_api.AffectedFiles():
        for line_num, line in f.ChangedContents():
            if _HasToDoWithNoBug(input_api, line):
                errors.append('%s:%s' % (f.LocalPath(), line_num))
    if not errors:
        return []

    plural_suffix = '' if len(errors) == 1 else 's'
    error_message = '\n'.join([
        'Found TO'
        'DO%(plural)s without bug number%(plural)s (expected format '
        'is \"TO'
        'DO(crbug.com/######)\":' % {
            'plural': plural_suffix
        }
    ] + errors) + '\n'

    return [output_api.PresubmitError(error_message)]


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
    error_message = '\n'.join([
        'Please use backticks "`" instead of pipes "|" if you need to quote'
        ' variable names and symbols in comments.\n'
        'Found potential uses of pipes in:'
    ] + errors) + '\n'

    return [output_api.PresubmitPromptWarning(error_message)]


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
    error_message = ('Found boxed BOOL%(plural)s. '
                     'Prefer @YES or @NO in ios code:' % {
                         'plural': plural_suffix
                     })

    return [output_api.PresubmitPromptWarning(error_message, items=errors)]

def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CheckBugInToDo(input_api, output_api))
    results.extend(_CheckNullabilityAnnotations(input_api, output_api))
    results.extend(_CheckHasNoIncludeDirectives(input_api, output_api))
    results.extend(_CheckHasNoPipeInComment(input_api, output_api))
    results.extend(_CheckHasNoBoxedBOOL(input_api, output_api))
    return results
