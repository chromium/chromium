# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for Chromium media component.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os
import re

PRESUBMIT_VERSION = '2.0.0'

# Well-defined simple classes containing only <= 4 ints, or <= 2 floats.
BASE_TIME_TYPES = [
    'base::Time',
    'base::TimeDelta',
    'base::TimeTicks',
]

BASE_TIME_TYPES_RE = re.compile(r'\bconst (%s)&' % '|'.join(BASE_TIME_TYPES))


def _FilterFile(affected_file):
    """Return true if the file could contain code requiring a presubmit check."""
    return affected_file.LocalPath().endswith(
        ('.h', '.cc', '.cpp', '.cxx', '.mm'))


def _CheckForUseOfWrongClock(input_api, output_api):
    """Make sure new lines of media code don't use a clock susceptible to skew."""

    # Regular expression that should detect any explicit references to the
    # base::Time type (or base::Clock/DefaultClock), whether in using decls,
    # typedefs, or to call static methods.
    base_time_type_pattern = r'(^|\W)base::(Time|Clock|DefaultClock)(\W|$)'

    # Regular expression that should detect references to the base::Time class
    # members, such as a call to base::Time::Now.
    base_time_member_pattern = r'(^|\W)(Time|Clock|DefaultClock)::'

    # Regular expression to detect "using base::Time" declarations.  We want to
    # prevent these from triggering a warning.  For example, it's perfectly
    # reasonable for code to be written like this:
    #
    #   using base::Time;
    #   ...
    #   int64_t foo_us = foo_s * Time::kMicrosecondsPerSecond;
    using_base_time_decl_pattern = r'^\s*using\s+(::)?base::Time\s*;'

    # Regular expression to detect references to the kXXX constants in the
    # base::Time class.  We want to prevent these from triggering a warning.
    base_time_konstant_pattern = r'(^|\W)Time::k\w+'

    # Regular expression to detect usage of openscreen clock types, which are
    # allowed depending on DEPS rules.
    openscreen_time_pattern = r'(^|\W)openscreen::Clock\s*'

    problem_re = input_api.re.compile(r'(' + base_time_type_pattern + r')|(' +
                                      base_time_member_pattern + r')')
    exception_re = input_api.re.compile(r'(' + using_base_time_decl_pattern +
                                        r')|(' + base_time_konstant_pattern +
                                        r')|(' + openscreen_time_pattern +
                                        r')')
    problems = []
    for f in input_api.AffectedSourceFiles(_FilterFile):
        for line_number, line in f.ChangedContents():
            if problem_re.search(line):
                if not exception_re.search(line):
                    problems.append('  %s:%d\n    %s' %
                                    (f.LocalPath(), line_number, line.strip()))

    if problems:
        return [
            output_api.PresubmitPromptOrNotify(
                'You added one or more references to the base::Time class and/or one\n'
                'of its member functions (or base::Clock/DefaultClock). In media\n'
                'code, it is rarely correct to use a clock susceptible to time skew!\n'
                'Instead, could you use base::TimeTicks to track the passage of\n'
                'real-world time?\n\n' + '\n'.join(problems))
        ]
    else:
        return []


def _CheckForHistogramOffByOne(input_api, output_api):
    """Make sure histogram enum maxes are used properly"""

    # A general-purpose chunk of regex to match whitespace and/or comments
    # that may be interspersed with the code we're interested in:
    comment = r'/\*.*?\*/|//[^\n]*'
    whitespace = r'(?:[\n\t ]|(?:' + comment + r'))*'

    # The name is assumed to be a literal string.
    histogram_name = r'"[^"]*"'

    # This can be an arbitrary expression, so just ensure it isn't a ; to prevent
    # matching past the end of this statement.
    histogram_value = r'[^;]*'

    # In parens so we can retrieve it for further checks.
    histogram_max = r'([^;,]*)'

    # This should match a uma histogram enumeration macro expression.
    uma_macro_re = input_api.re.compile(r'\bUMA_HISTOGRAM_ENUMERATION\(' +
                                        whitespace + histogram_name + r',' +
                                        whitespace + histogram_value + r',' +
                                        whitespace + histogram_max +
                                        whitespace + r'\)' + whitespace +
                                        r';(?:' + whitespace +
                                        r'\/\/ (PRESUBMIT_IGNORE_UMA_MAX))?')

    uma_max_re = input_api.re.compile(r'.*(?:Max|MAX).* \+ 1')

    problems = []

    for f in input_api.AffectedSourceFiles(_FilterFile):
        contents = input_api.ReadFile(f)

        # We want to match across lines, but still report a line number, so we keep
        # track of the line we're on as we search through the file.
        line_number = 1

        # We search the entire file, then check if any violations are in the changed
        # areas, this is inefficient, but simple. A UMA_HISTOGRAM_ENUMERATION call
        # will often span multiple lines, so finding a match looking just at the
        # deltas line-by-line won't catch problems.
        match = uma_macro_re.search(contents)
        while match:
            line_number += contents.count('\n', 0, match.start())
            max_arg = match.group(1)  # The third argument.

            if (not uma_max_re.match(max_arg)
                    and match.group(2) != 'PRESUBMIT_IGNORE_UMA_MAX'):
                uma_range = range(match.start(), match.end() + 1)
                # Check if any part of the match is in the changed lines:
                for num, line in f.ChangedContents():
                    if line_number <= num <= line_number + match.group().count(
                            '\n'):
                        problems.append('%s:%d' % (f, line_number))
                        break

            # Strip off the file contents up to the end of the match and update the
            # line number.
            contents = contents[match.end():]
            line_number += match.group().count('\n')
            match = uma_macro_re.search(contents)

    if problems:
        return [
            output_api.PresubmitError(
                'UMA_HISTOGRAM_ENUMERATION reports in src/media/ are expected to adhere\n'
                'to the following guidelines:\n'
                ' - The max value (3rd argument) should be an enum value equal to the\n'
                '   last valid value, e.g. FOO_MAX = LAST_VALID_FOO.\n'
                ' - 1 must be added to that max value.\n'
                'Contact dalecurtis@chromium.org if you have questions.',
                problems)
        ]

    return []


def _CheckPassByValue(input_api, output_api):
    """Check that base::Time and derived classes are passed by value, and not by
  const reference """

    problems = []

    for f in input_api.AffectedSourceFiles(_FilterFile):
        for line_number, line in f.ChangedContents():
            if BASE_TIME_TYPES_RE.search(line):
                problems.append('%s:%d' % (f, line_number))

    if problems:
        return [
            output_api.PresubmitError(
                'base::Time and derived classes should be passed by value and not by\n'
                'const ref, see base/time/time.h for more information.',
                problems)
        ]
    return []


def _CheckForUseOfLazyInstance(input_api, output_api):
    """Check that base::LazyInstance is not used."""

    problems = []

    lazy_instance_re = re.compile(r'(^|\W)base::LazyInstance<')

    for f in input_api.AffectedSourceFiles(_FilterFile):
        for line_number, line in f.ChangedContents():
            if lazy_instance_re.search(line):
                problems.append('%s:%d' % (f, line_number))

    if problems:
        return [
            output_api.PresubmitError(
                'base::LazyInstance is deprecated; use a thread safe static.',
                problems)
        ]
    return []


def _CheckNoLoggingOverrideInHeaders(input_api, output_api):
    """Checks to make sure no .h files include logging_override_if_enabled.h."""
    files = []
    pattern = input_api.re.compile(
        r'^#include\s*"media/base/logging_override_if_enabled.h"',
        input_api.re.MULTILINE)
    for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
        if not f.LocalPath().endswith('.h'):
            continue
        contents = input_api.ReadFile(f)
        if pattern.search(contents):
            files.append(f)

    if len(files):
        return [
            output_api.PresubmitError(
                'Do not #include "logging_override_if_enabled.h" in header files, '
                'since it overrides DVLOG() in every file including the header. '
                'Instead, only include it in source files.', files)
        ]
    return []


def _CheckForNoV4L2AggregateInitialization(input_api, output_api):
    """Check that struct v4l2_* are not initialized as aggregates with a
  braced-init-list"""

    problems = []

    v4l2_aggregate_initializer_re = re.compile(
        r'(^|\W)struct.+v4l2_.+=.+{+}+;')

    for f in input_api.AffectedSourceFiles(_FilterFile):
        for line_number, line in f.ChangedContents():
            if v4l2_aggregate_initializer_re.search(line):
                problems.append('%s:%d' % (f, line_number))

    if problems:
        return [
            output_api.PresubmitPromptWarning(
                'Avoid initializing V4L2 structures with braced-init-lists, i.e. as '
                'aggregates. V4L2 structs often contain unions of various sized members: '
                'when a union is initialized by aggregate initialization, only the first '
                'non-static member is initialized, leaving other members unitialized if '
                'they are larger. Use memset instead.', problems)
        ]
    return []


def _CheckChangeInBundle(input_api, output_api):
    import sys
    old_sys_path = sys.path[:]
    results = []
    try:
        sys.path.append(input_api.change.RepositoryRoot())
        from build.ios import presubmit_support
        results += presubmit_support.CheckBundleData(input_api, output_api,
                                                     'unit_tests_bundle_data')
    finally:
        sys.path = old_sys_path
    return results


def _CheckIfGenerateGnTestsFail(input_api, output_api):
    """Error if generate_gn.py was changed and tests are now failing."""
    should_run_tests = False
    generate_gn_re = re.compile(r'.*generate_gn.*\.py$')
    for f in input_api.AffectedFiles():
        if generate_gn_re.match(f.LocalPath()):
            should_run_tests = True
            break

    errors = []
    if should_run_tests:
        errors += input_api.RunTests(
            input_api.canned_checks.GetUnitTests(
                input_api,
                output_api, [
                    os.path.join('ffmpeg', 'scripts',
                                 'generate_gn_unittest_wrapper.py')
                ],
                run_on_python2=False,
                run_on_python3=True,
                skip_shebang_check=False))

    return errors


def _CheckChange(input_api, output_api):
    results = []
    results.extend(_CheckForUseOfWrongClock(input_api, output_api))
    results.extend(_CheckPassByValue(input_api, output_api))
    results.extend(_CheckForHistogramOffByOne(input_api, output_api))
    results.extend(_CheckForUseOfLazyInstance(input_api, output_api))
    results.extend(_CheckNoLoggingOverrideInHeaders(input_api, output_api))
    results.extend(
        _CheckForNoV4L2AggregateInitialization(input_api, output_api))
    results.extend(_CheckChangeInBundle(input_api, output_api))
    results.extend(_CheckIfGenerateGnTestsFail(input_api, output_api))
    return results


def CheckChangeOnUpload(input_api, output_api):
    return _CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CheckChange(input_api, output_api)
