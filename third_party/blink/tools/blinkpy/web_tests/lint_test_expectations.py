# Copyright (C) 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import contextlib
import json
import logging
import optparse
import posixpath
import re
import traceback
import os
from typing import List, Optional

from blinkpy.common import exit_codes
from blinkpy.common.host import Host
from blinkpy.common.path_finder import PathFinder
from blinkpy.common.system.log_utils import configure_logging
from blinkpy.web_tests.models.test_expectations import (TestExpectations,
                                                        ParseError)
from blinkpy.web_tests.models.typ_types import ResultType
from blinkpy.web_tests.port.base import Port
from blinkpy.web_tests.port.factory import platform_options

_log = logging.getLogger(__name__)


@contextlib.contextmanager
def _capture_parse_error(failures):
    try:
        yield
    except ParseError as error:
        messages = str(error).strip().split('\n\n')
        # Filename is already included in the individual error messages.
        exclude_pattern = re.compile(
            'Parsing file .* produced following errors')
        failures.extend(message for message in messages
                        if not exclude_pattern.fullmatch(message))


def lint(host, options):
    # The checks and list of expectation files are generally not
    # platform-dependent. Still, we need a port to identify test types and
    # manipulate virtual test paths.
    finder = PathFinder(host.filesystem)
    # Add all extra expectation files to be linted.
    options.additional_expectations.extend([
        finder.path_from_web_tests('MobileTestExpectations'),
        finder.path_from_web_tests('WebGPUExpectations'),
    ])
    port = host.port_factory.get(options=options)

    failures = []
    warnings = []
    all_system_specifiers = set()
    all_build_specifiers = set(port.ALL_BUILD_TYPES)

    expectations_dict = port.all_expectations_dict()
    for path in port.extra_expectations_files():
        if host.filesystem.exists(path):
            expectations_dict[path] = host.filesystem.read_text_file(path)

    # Needed for checking if a test is a slow test. Construction is somewhat
    # expensive because it parses the main `TestExpectations` file, so reuse it
    # across all files checked.
    all_test_expectations = None
    with _capture_parse_error(failures):
        all_test_expectations = TestExpectations(port)

    for path, content in expectations_dict.items():
        # Create a TestExpectations instance and see if an exception is raised
        with _capture_parse_error(failures):
            test_expectations = TestExpectations(
                port, expectations_dict={path: content})
            # Check each expectation for issues
            f, w = _check_expectations(host, port, path, test_expectations,
                                       options, all_test_expectations)
            failures += f
            warnings += w

    return failures, warnings


def _check_test_existence(host, port, path, expectations):
    failures = []
    warnings = []
    for exp in expectations:
        if not exp.test:
            continue
        if exp.is_glob:
            test_name = exp.test[:-1]
        else:
            test_name = exp.test
        possible_error = "{}:{} Test does not exist: {}".format(
            host.filesystem.basename(path), exp.lineno, exp.test)
        if not port.test_exists(test_name):
            failures.append(possible_error)
    return failures, warnings


def _check_directory_glob(host, port, path, expectations):
    failures = []
    for exp in expectations:
        if not exp.test or exp.is_glob:
            continue

        test_name = exp.test
        index = test_name.find('?')
        if index != -1:
            test_name = test_name[:index]

        if port.test_isdir(test_name):
            error = (
                ("%s:%d Expectation '%s' is for a directory, however "
                 "the name in the expectation does not have a glob in the end")
                % (host.filesystem.basename(path), exp.lineno, test_name))
            failures.append(error)

    return failures


def _check_redundant_virtual_expectations(host, port, path, expectations):
    # FlagExpectations are not checked because something like the following in
    # a flag-specific expectations file looks redundant but is needed to
    # override the virtual expectations in TestExpectations. For example, in
    # the main TestExpectations:
    #   foo/bar/test.html [ Failure ]
    #   virtual/suite/foo/bar/test.html [ Timeout ]
    # and in a flag expectation file, we want to override both to [ Crash ]:
    #   foo/bar/test.html [ Crash ]
    #   virtual/suite/foo/bar/test.html [ Crash ]
    if 'FlagExpectations' in path:
        return []

    failures = []
    base_expectations_by_test = {}
    virtual_expectations = []
    virtual_globs = []
    for exp in expectations:
        if not exp.test:
            continue
        # TODO(crbug.com/1080691): For now, ignore redundant entries created by
        # WPT import.
        if port.is_wpt_test(exp.test):
            continue

        base_test = port.lookup_virtual_test_base(exp.test)
        if base_test:
            virtual_expectations.append((exp, base_test))
            if exp.is_glob:
                virtual_globs.append(exp.test[:-1])
        else:
            base_expectations_by_test.setdefault(exp.test, []).append(exp)

    for (exp, base_test) in virtual_expectations:
        for base_exp in base_expectations_by_test.get(base_test, []):
            if (base_exp.results == exp.results
                    and base_exp.is_slow_test == exp.is_slow_test
                    and base_exp.tags.issubset(exp.tags)
                    and base_exp.reason == exp.reason
                    # Don't report redundant expectation in the following case
                    # bar/test.html [ Failure ]
                    # virtual/foo/bar/* [ Pass ]
                    # virtual/foo/bar/test.html [ Failure ]
                    # For simplicity, tags of the glob expectations are ignored.
                    and not any(exp.test != glob and exp.test.startswith(glob)
                                for glob in virtual_globs)):
                error = "{}:{} Expectation '{}' is redundant with '{}' in line {}".format(
                    host.filesystem.basename(path), exp.lineno, exp.test,
                    base_test, base_exp.lineno)
                # TODO(crbug.com/1080691): Change to error once it's fixed.
                failures.append(error)

    return failures


def _check_not_slow_and_timeout(host, port, path, expectations,
                                all_test_expectations):
    # only do check for web tests, so that we don't impact test coverage
    # for other test suites
    if (not path.endswith('TestExpectations') and
        not path.endswith('SlowTests')):
        return []
    # Not all default expectation files could be parsed, so this check cannot
    # run.
    if not all_test_expectations:
        return []

    rv = []

    for exp in expectations:
        if (ResultType.Timeout in exp.results and len(exp.results) == 1 and
            (all_test_expectations.get_expectations(exp.test).is_slow_test
             or port.is_slow_wpt_test(exp.test))):
            error = "{}:{} '{}' is a [ Slow ] and [ Timeout ] test: you must add [ Skip ] (see crrev.com/c/3381301).".format(
                host.filesystem.basename(path), exp.lineno, exp.test)
            rv.append(error)

    return rv


def _check_never_fix_tests(host, port, path, expectations):
    if not path.endswith('NeverFixTests'):
        return []

    def pass_validly_overrides_skip(pass_exp, skip_exp):
        if skip_exp.results != set([ResultType.Skip]):
            return False
        if not skip_exp.tags.issubset(pass_exp.tags):
            return False
        if skip_exp.is_glob and pass_exp.test.startswith(skip_exp.test[:-1]):
            return True
        return False

    failures = []
    for i in range(len(expectations)):
        exp = expectations[i]
        if (exp.results != set([ResultType.Pass])
                and exp.results != set([ResultType.Skip])):
            error = "{}:{} Only one of [ Skip ] and [ Pass ] is allowed".format(
                host.filesystem.basename(path), exp.lineno)
            failures.append(error)
            continue
        if exp.is_default_pass or exp.results != set([ResultType.Pass]):
            continue
        if any(
                pass_validly_overrides_skip(exp, expectations[j])
                for j in range(i - 1, 0, -1)):
            continue

        if port.lookup_virtual_test_base(exp.test):
            error = (
                "{}:{} {}: Please use 'exclusive_tests' in VirtualTestSuites to"
                " skip base tests of a virtual suite".format(
                    host.filesystem.basename(path), exp.lineno, exp.test))
        else:
            error = (
                "{}:{} {}: The [ Pass ] entry must override a previous [ Skip ]"
                " entry with a more specific test name or tags".format(
                    host.filesystem.basename(path), exp.lineno, exp.test))
        failures.append(error)
    return failures


def _check_skip_in_test_expectations(host, path, expectations):
    if not path.endswith('TestExpectations'):
        return []

    failures = []
    for exp in expectations:
        if exp.results == set([ResultType.Skip]):
            error = (
                '{}:{} Single [ Skip ] is not allowed in TestExpectations. '
                'See comments at the beginning of the file for details.'.
                format(host.filesystem.basename(path), exp.lineno))
            failures.append(error)
    return failures


def _check_expectations(host, port, path, test_expectations, options,
                        all_test_expectations):
    # Check for original expectation lines (from get_updated_lines) instead of
    # expectations filtered for the current port (test_expectations).
    expectations = test_expectations.get_updated_lines(path)
    failures, warnings = _check_test_existence(
        host, port, path, expectations)
    failures.extend(_check_directory_glob(host, port, path, expectations))
    failures.extend(
        _check_not_slow_and_timeout(host, port, path, expectations,
                                    all_test_expectations))
    failures.extend(_check_never_fix_tests(host, port, path, expectations))
    failures.extend(
        _check_stable_webexposed_not_disabled(host, path, expectations))
    failures.extend(_check_skip_in_test_expectations(host, path, expectations))
    # TODO(crbug.com/1080691): Change this to failures once
    # wpt_expectations_updater is fixed.
    warnings.extend(
        _check_redundant_virtual_expectations(host, port, path, expectations))
    return failures, warnings


def _check_stable_webexposed_not_disabled(host, path, expectations):
    if not host.filesystem.basename(path) == "TestExpectations":
        return []

    failures = []

    for exp in expectations:
        if exp.test.startswith("virtual/stable/webexposed") \
                and exp.results != set([ResultType.Pass]) and not exp.is_default_pass:
            error = "{}:{} {}: test should not be disabled " \
                    "because it protects against API changes.".format(
                        host.filesystem.basename(path), exp.lineno, exp.to_string())
            failures.append(error)

    return failures


def check_virtual_test_suites(host, options):
    port = host.port_factory.get(options=options)
    fs = host.filesystem
    web_tests_dir = port.web_tests_dir()
    virtual_suites = port.virtual_test_suites()
    virtual_suites.sort(key=lambda s: s.full_prefix)
    max_suite_length = 48

    wpt_tests = set()
    for wpt_dir in port.WPT_DIRS:
        wpt_tests.update(
            posixpath.join(wpt_dir, url)
            for url in port.wpt_manifest(wpt_dir).all_urls())

    failures = []
    for suite in virtual_suites:
        suite_comps = suite.full_prefix.split(port.TEST_PATH_SEPARATOR)
        prefix = suite_comps[1]
        owners = suite.owners
        normalized_bases = [port.normalize_test_name(b) for b in suite.bases]
        normalized_bases.sort()
        for i in range(1, len(normalized_bases)):
            for j in range(0, i):
                if normalized_bases[i].startswith(normalized_bases[j]):
                    failure = 'Base "{}" starts with "{}" in the same virtual suite "{}", so is redundant.'.format(
                        normalized_bases[i], normalized_bases[j], prefix)
                    failures.append(failure)

        # A virtual test suite needs either
        # - a top-level README.md (e.g. virtual/foo/README.md)
        # - a README.txt for each covered directory (e.g.
        #   virtual/foo/http/tests/README.txt, virtual/foo/fast/README.txt, ...)
        comps = [web_tests_dir] + suite_comps + ['README.md']
        path_to_readme_md = fs.join(*comps)
        for base in suite.bases:
            if not base:
                failure = 'Base value in virtual suite "{}" should not be an empty string'.format(
                    prefix)
                failures.append(failure)
                continue
            base_comps = base.split(port.TEST_PATH_SEPARATOR)
            absolute_base = port.abspath_for_test(base)
            # Also, allow any WPT URLs that are valid generated tests but
            # aren't test files (e.g., `.any.js` and variants).
            if fs.isfile(absolute_base) or base in wpt_tests:
                del base_comps[-1]
            elif not fs.isdir(absolute_base):
                failure = 'Base "{}" in virtual suite "{}" must refer to a real file or directory'.format(
                    base, prefix)
                failures.append(failure)
                continue

            if port.skipped_due_to_exclusive_virtual_tests(suite.full_prefix +
                                                           base):
                failure = (
                    'Base "{}" in virtual suite "{}" is in exclusive_tests '
                    'of other virtual suites. It will be skipped. '
                    'Either remove the base or list the base in this '
                    'suite\'s exclusive_tests.'.format(base, prefix))
                failures.append(failure)
                continue

            comps = [web_tests_dir] + suite_comps + base_comps + ['README.txt']
            path_to_readme_txt = fs.join(*comps)
            if (not fs.exists(path_to_readme_md)
                    and not fs.exists(path_to_readme_txt)):
                failure = '"{}" and "{}" are both missing (each virtual suite must have one).'.format(
                    path_to_readme_txt, path_to_readme_md)
                failures.append(failure)

        for exclusive_test in suite.exclusive_tests:
            if not fs.exists(port.abspath_for_test(
                    exclusive_test)) and base not in wpt_tests:
                failure = 'Exclusive_tests entry "{}" in virtual suite "{}" must refer to a real file or directory'.format(
                    exclusive_test, prefix)
                failures.append(failure)
            elif not any(
                    port.normalize_test_name(exclusive_test).startswith(base)
                    for base in normalized_bases):
                failure = 'Exclusive_tests entry "{}" in virtual suite "{}" is not a subset of bases'.format(
                    exclusive_test, prefix)
                failures.append(failure)

        if not owners:
            failure = 'Virtual suite name "{}" has no owner.'.format(prefix)
            failures.append(failure)

        if len(prefix) > max_suite_length:
            failures = 'Virtual suite name "{}" is over the "{}" filename length limit'.format(
                prefix, max_suite_length)

    return failures


def check_test_lists(host, options):
    port = host.port_factory.get(options=options)
    path = host.filesystem.join(port.web_tests_dir(), 'TestLists')
    test_lists_files = host.filesystem.listdir(path)
    failures = []
    for test_lists_file in test_lists_files:
        test_lists = host.filesystem.read_text_file(
            host.filesystem.join(port.web_tests_dir(), 'TestLists',
                                 test_lists_file))
        line_number = 0
        parsed_lines = {}
        for line in test_lists.split('\n'):
            line_number += 1
            line = line.split('#')[0].strip()
            if line and line[-1] == '*':
                line = line[:-1]
            if not line:
                continue
            if line in parsed_lines:
                failures.append(
                    '%s:%d duplicate with line %d: %s' %
                    (test_lists_file, line_number, parsed_lines[line], line))
            elif not port.test_exists(line):
                failures.append('%s:%d Test does not exist: %s' %
                                (test_lists_file, line_number, line))
            parsed_lines[line] = line_number

    return failures


def run_checks(host, options):
    failures = []
    warnings = []
    if os.getcwd().startswith('/google/cog/cloud'):
        _log.info('Skipping run_checks for cog workspace')
        return 0
    f, w = lint(host, options)
    failures += f
    warnings += w
    failures.extend(check_virtual_test_suites(host, options))
    failures.extend(check_test_lists(host, options))

    if options.json:
        with open(options.json, 'w') as f:
            json.dump(failures, f)

    # Dedup identical errors/warnings.
    for failure in sorted(set(failures)):
        _log.error(failure)
        _log.error('')
    for warning in sorted(set(warnings)):
        _log.warning(warning)
        _log.warning('')

    if failures:
        _log.error('Lint failed.')
        return 1
    elif warnings:
        _log.warning('Lint succeeded with warnings.')
        return 2
    else:
        _log.info('Lint succeeded.')
        return 0


def main(argv, stderr, host=None):
    parser = optparse.OptionParser(
        option_list=platform_options(use_globs=True))
    parser.add_option('--json', help='Path to JSON output file')
    parser.add_option(
        '--verbose',
        action='store_true',
        default=False,
        help='log extra details that may be helpful when debugging')
    parser.add_option(
        '--additional-expectations',
        action='append',
        default=[],
        help='paths to additional expectation files to lint.')

    options, _ = parser.parse_args(argv)

    if not host:
        if options.platform and 'test' in options.platform:
            # It's a bit lame to import mocks into real code, but this allows the user
            # to run tests against the test platform interactively, which is useful for
            # debugging test failures.
            from blinkpy.common.host_mock import MockHost
            host = MockHost()
        else:
            host = Host()

    if options.verbose:
        configure_logging(logging_level=logging.DEBUG, stream=stderr)
        # Print full stdout/stderr when a command fails.
        host.executive.error_output_limit = None
    else:
        # PRESUBMIT.py relies on our output, so don't include timestamps.
        configure_logging(logging_level=logging.WARNING,
                          stream=stderr,
                          include_time=False)

    try:
        exit_status = run_checks(host, options)
    except KeyboardInterrupt:
        exit_status = exit_codes.INTERRUPTED_EXIT_STATUS
    except Exception as error:  # pylint: disable=broad-except
        print('\n%s raised: %s' % (error.__class__.__name__, error), stderr)
        traceback.print_exc(file=stderr)
        exit_status = exit_codes.EXCEPTIONAL_EXIT_STATUS

    return exit_status
