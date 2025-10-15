# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for ios.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os
import subprocess
import xml.etree.ElementTree as ElementTree

NULLABILITY_PATTERN = r'(nonnull|nullable|_Nullable|_Nonnull)'
TODO_PATTERN = r'TO[D]O\(([^\)]*)\)'
BUG_PATTERN = r'^(crbug\.com|b)/\d+$'
DEPRECATED_BUG_PATTERN = r'^b/\d+$'
INCLUDE_PATTERN = r'^#include'
PIPE_IN_COMMENT_PATTERN = r'//.*[^|]\|(?!\|)'
IOS_PACKAGE_PATTERN = r'^ios'
BOXED_BOOL_PATTERN = r'@\((YES|NO)\)'
USER_DEFAULTS_PATTERN = r'\[NSUserDefaults standardUserDefaults]'

# Color management constants
COLOR_SHARED_DIR = 'ios/chrome/common/ui/colors/'
COLOR_FILE_PATTERN = '.colorset/Contents.json'


def FormatMessageWithFiles(message, errors):
    """Helper to format warning/error messages with affected files."""
    if not errors:
        return message
    return '\n'.join([message + '\n\nAffected file(s):'] + errors) + '\n'

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

def _CheckNoTearDownEGTest(input_api, output_api):
    """ Checks that `- (void)tearDown {` is not present in an egtest.mm"""
    errors = []
    for f in input_api.AffectedFiles():
        if not '_egtest.' in f.LocalPath():
            continue
        for line_num, line in f.ChangedContents():
            if line.startswith("- (void)tearDown {"):
                errors.append('%s:%s' % (f.LocalPath(), line_num))

    if not errors:
        return []
    warning_message = '\n'.join([
        'To support hermetic EarlGrey test cases, tearDown has been renamed '
        'to tearDownHelper, and will soon be removed. If tearDown is really '
        'necessary for this test, please use addTeardownBlock'
    ] + errors) + '\n'

    return [output_api.PresubmitError(warning_message)]


def _IsAlphabeticallySortedXML(file):
    """Check that the `file` is alphabetically sorted"""
    parser = ElementTree.XMLParser(target=ElementTree.TreeBuilder(
        insert_comments=True))
    with open(file, 'r', encoding='utf8') as xml_file:
        tree = ElementTree.parse(xml_file, parser)
    root = tree.getroot()

    original_tree_string = ElementTree.tostring(root, encoding='utf8')

    messages_element = tree.findall('.//messages')[0]
    messages = messages_element.findall('message')
    messages.sort(key=lambda message: message.attrib["name"])
    for message in messages:
        messages_element.remove(message)
    for message in messages:
        messages_element.append(message)
    ordered_tree_string = ElementTree.tostring(root, encoding='utf8')
    return ordered_tree_string == original_tree_string


def _CheckOrderedStringFile(input_api, output_api):
    """ Checks that the string files are alphabetically ordered"""
    errors = []
    for f in input_api.AffectedFiles(include_deletes=False):
        if not f.LocalPath().endswith("_strings.grd"):
            continue
        if not _IsAlphabeticallySortedXML(f.AbsoluteLocalPath()):
            errors.append('  python3 ios/tools/order_string_file.py ' +
                          f.LocalPath())

    if not errors:
        return []
    warning_message = '\n'.join(
        ['Files not alphabetically sorted, try running:'] + errors) + '\n'

    return [output_api.PresubmitPromptWarning(warning_message)]


def _CheckOrderedFlagsFile(input_api, output_api):
    """ Checks that the flag description files are alphabetically ordered"""
    h_file = None
    cc_file = None
    for f in input_api.AffectedFiles(include_deletes=False):
        if f.LocalPath().endswith('ios_chrome_flag_descriptions.h'):
            h_file = f.LocalPath()
        elif f.LocalPath().endswith('ios_chrome_flag_descriptions.cc'):
            cc_file = f.LocalPath()

    if h_file or cc_file:
        try:
            command = [
                input_api.python3_executable, 'tools/order_flags.py', '--check'
            ]
            if h_file:
                command.extend(['--h-file', h_file])
            if cc_file:
                command.extend(['--cc-file', cc_file])
            subprocess.check_output(command, stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as e:
            message = 'Flag description files not alphabetically sorted.\n'
            message += e.output.decode('utf-8')
            message += '\nPlease run: python3 ios/tools/order_flags.py'
            return [output_api.PresubmitError(message)]

    return []


def _CheckNotUsingNSUserDefaults(input_api, output_api):
    """ Checks the added code to limit new usage of NSUserDefaults """
    user_defaults_regex = input_api.re.compile(USER_DEFAULTS_PATTERN)

    errors = []
    for f in input_api.AffectedFiles():
        if (not f.LocalPath().endswith('.mm')):
            continue
        for line_num, line in f.ChangedContents():
            if user_defaults_regex.search(line):
                errors.append('%s:%s' % (f.LocalPath(), line_num))

    if not errors:
        return []
    warning_message = '\n'.join([
        'A new use of NSUserDefaults was added. If this is a newly added key '
        'consider storing it to PrefService instead.'
    ] + errors) + '\n'

    return [output_api.PresubmitPromptWarning(warning_message)]


def _CheckNewColorIntroduction(input_api, output_api):
    """Checks for new or modified colorset files.

    Ensures colors are properly added to the shared directory.
    """
    results = []

    affected_files = [
        f for f in input_api.AffectedFiles()
        if f.LocalPath().endswith(COLOR_FILE_PATTERN)
    ]

    warnings = {
        'shared_added': [],
        'shared_modified': [],
        'other_modified': []
    }
    errors = []

    for affected_file in affected_files:
        action = affected_file.Action()
        local_path = affected_file.LocalPath()
        file_path_error = '%s' % (affected_file.LocalPath())

        if COLOR_SHARED_DIR in local_path:
            if action == 'A':
                warnings['shared_added'].append(file_path_error)
            elif action == 'M':
                warnings['shared_modified'].append(file_path_error)
        else:
            if action == 'A':
                errors.append(file_path_error)
            elif action == 'M':
                warnings['other_modified'].append(file_path_error)

    output = []

    if errors:
        error_message = ('New color(s) must be added to the %s directory.' %
                         COLOR_SHARED_DIR)
        output.append(
            output_api.PresubmitError(
                FormatMessageWithFiles(error_message, errors)))

    warning_message = ('Please ensure the color does not already exist in the '
                       'shared %s directory.' % COLOR_SHARED_DIR)

    if warnings['shared_added']:
        shared_added_message = ('New color(s) added in %s. %s' %
                                (COLOR_SHARED_DIR, warning_message))
        output.append(
            output_api.PresubmitPromptWarning(
                FormatMessageWithFiles(shared_added_message,
                                       warnings['shared_added'])))

    if warnings['shared_modified']:
        shared_modified_message = ('Color(s) modified in %s. %s' %
                                   (COLOR_SHARED_DIR, warning_message))
        output.append(
            output_api.PresubmitPromptWarning(
                FormatMessageWithFiles(shared_modified_message,
                                       warnings['shared_modified'])))

    if warnings['other_modified']:
        modified_message = ('Color(s) modified. %s' % warning_message)
        output.append(
            output_api.PresubmitPromptWarning(
                FormatMessageWithFiles(modified_message,
                                       warnings['other_modified'])))

    return output

def _CheckStyleESLint(input_api, output_api):
    results = []

    try:
        import sys
        old_sys_path = sys.path[:]
        cwd = input_api.PresubmitLocalPath()
        sys.path += [input_api.os_path.join(cwd, '..', 'tools')]
        from web_dev_style import presubmit_support
        results += presubmit_support.CheckStyleESLint(input_api, output_api)
    finally:
        sys.path = old_sys_path

    return results

def _CheckUIGraphicsBeginImageContextWithOptions(input_api, output_api):
    """ Checks that UIGraphicsBeginImageContextWithOptions is not used"""
    deprecated_regex = input_api.re.compile(
        r'UIGraphicsBeginImageContextWithOptions\(')

    errors = []
    for f in input_api.AffectedFiles():
        if (not f.LocalPath().endswith('.mm')):
            continue
        for line_num, line in f.ChangedContents():
            if deprecated_regex.search(line):
                errors.append('%s:%s' % (f.LocalPath(), line_num))

    if not errors:
        return []
    error_message = '\n'.join([
        'UIGraphicsBeginImageContextWithOptions is deprecated, use '
        'UIGraphicsImageRenderer instead.'
    ] + errors) + '\n'

    return [output_api.PresubmitError(error_message)]

def CheckChange(input_api, output_api):
    results = []
    results.extend(_CheckBugInToDo(input_api, output_api))
    results.extend(_CheckNullabilityAnnotations(input_api, output_api))
    results.extend(_CheckHasNoIncludeDirectives(input_api, output_api))
    results.extend(_CheckHasNoPipeInComment(input_api, output_api))
    results.extend(_CheckHasNoBoxedBOOL(input_api, output_api))
    results.extend(_CheckNoTearDownEGTest(input_api, output_api))
    results.extend(_CheckCanImproveTestUsingExpectNSEQ(input_api, output_api))
    results.extend(_CheckOrderedStringFile(input_api, output_api))
    results.extend(_CheckOrderedFlagsFile(input_api, output_api))
    results.extend(_CheckNotUsingNSUserDefaults(input_api, output_api))
    results.extend(_CheckNewColorIntroduction(input_api, output_api))
    results.extend(_CheckStyleESLint(input_api, output_api))
    results.extend(
        _CheckUIGraphicsBeginImageContextWithOptions(input_api, output_api))
    return results

def CheckChangeOnUpload(input_api, output_api):
    return CheckChange(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
    return CheckChange(input_api, output_api)
