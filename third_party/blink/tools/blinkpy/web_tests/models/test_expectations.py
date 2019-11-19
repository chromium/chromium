# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""A helper class for reading in and dealing with tests expectations for web tests."""

from collections import defaultdict

import logging
import re

from blinkpy.common.path_finder import PathFinder
from blinkpy.web_tests.models.test_configuration import TestConfigurationConverter

_log = logging.getLogger(__name__)


# Test expectation and specifier constants.
#
# FIXME: range() starts with 0 which makes if expectation checks harder
# as PASS is 0.
(PASS, FAIL, TIMEOUT, CRASH, SKIP, SLOW, FLAKY) = range(7)

WEBKIT_BUG_PREFIX = 'webkit.org/b/'
CHROMIUM_BUG_PREFIX = 'crbug.com/'
SKIA_BUG_PREFIX = 'skbug.com/'
V8_BUG_PREFIX = 'code.google.com/p/v8/issues/detail?id='
NAMED_BUG_PREFIX = 'Bug('


class ParseError(Exception):

    def __init__(self, warnings):
        super(ParseError, self).__init__()
        self.warnings = warnings

    def __str__(self):
        return '\n'.join(map(str, self.warnings))

    def __repr__(self):
        return 'ParseError(warnings=%s)' % self.warnings


_PLATFORM_TOKENS_LIST = [
    'Android',
    'Fuchsia',
    'IOS', 'IOS12.2', 'IOS13.0',
    'Linux',
    'Mac', 'Mac10.10', 'Mac10.11', 'Retina', 'Mac10.12', 'Mac10.13',
    'Win', 'Win7', 'Win10'
]

_BUILD_TYPE_TOKEN_LIST = [
    'Release',
    'Debug',
]

_SPECIFIER_GROUPS = [
    set(s.upper() for s in _PLATFORM_TOKENS_LIST),
    set(s.upper() for s in _BUILD_TYPE_TOKEN_LIST)
]


class AllTests(object):
    """A class to query for path-prefix matches against the list of all tests."""
    def __init__(self, list_of_tests):
        self._tree = dict()
        for test in list_of_tests:
            assert not test.startswith('/')
            assert not test.endswith('/')
            assert test.replace('//', '/') == test
            AllTests._add_path_to_tree(self._tree, test.split('/'))

    def find_matching_tests(self, path_prefix):
        assert not path_prefix.startswith('/')
        assert path_prefix.replace('//', '/') == path_prefix

        subtree = AllTests._find_subtree_with_prefix(self._tree, path_prefix.rstrip('/').split('/'))
        if subtree is None:
            # No match found.
            return []
        if not subtree:
            # We found a leaf node, an exact match on |path_prefix|.
            return [path_prefix]
        return AllTests._all_paths_under_subtree(subtree, path_prefix)

    @staticmethod
    def _find_subtree_with_prefix(subtree, path_list):
        # Reached the end of the path, we matched to this point in the
        # dictionary tree.
        if not path_list:
            return subtree
        path_component = path_list[0]
        # A component of the path does not exist in the dictionary tree.
        if not path_component in subtree:
            return None
        path_remainder = path_list[1:]
        return AllTests._find_subtree_with_prefix(subtree[path_component], path_remainder)

    @staticmethod
    def _all_paths_under_subtree(subtree, prefix):
        if not subtree:
            return []

        if prefix and not prefix.endswith('/'):
            prefix = prefix + '/'

        paths = []
        for child_path, child_tree in subtree.iteritems():
            if not child_tree:
                paths.append(prefix + child_path)
            else:
                paths.extend(AllTests._all_paths_under_subtree(child_tree, prefix + child_path))
        return paths

    @staticmethod
    def _add_path_to_tree(subtree, path_list):
        # When |path_list| is empty, we reached the end of the path,
        # so don't add anything more to |subtree|.
        if not path_list:
            # If subtree is not empty, then we have the same path listed
            # twice in the initial list of tests.
            assert len(subtree) == 0
            return

        path_component = path_list[0]
        if not path_component in subtree:
            subtree[path_component] = dict()

        path_remainder = path_list[1:]
        AllTests._add_path_to_tree(subtree[path_component], path_remainder)


class TestExpectationParser(object):
    """Provides parsing facilities for lines in the test_expectation.txt file."""

    # FIXME: Rename these to *_KEYWORD as in MISSING_KEYWORD above, but make
    # the case studdly-caps to match the actual file contents.
    PASS_EXPECTATION = 'pass'
    SKIP_MODIFIER = 'skip'
    SLOW_MODIFIER = 'slow'

    TIMEOUT_EXPECTATION = 'timeout'

    MISSING_BUG_WARNING = 'Test lacks BUG specifier.'

    def __init__(self, port, all_tests, is_lint_mode):
        self._port = port
        self._test_configuration_converter = TestConfigurationConverter(
            set(port.all_test_configurations()), port.configuration_specifier_macros())
        self._all_tests = AllTests(all_tests) if all_tests else None
        self._is_lint_mode = is_lint_mode

    def parse(self, filename, expectations_string):
        expectation_lines = []
        line_number = 0
        for line in expectations_string.split('\n'):
            line_number += 1
            test_expectation = TestExpectationLine.tokenize_line(filename, line, line_number, self._port)
            self._parse_line(test_expectation)
            expectation_lines.append(test_expectation)

        if self._is_lint_mode:
            self._validate_specifiers(expectation_lines)
        return expectation_lines

    def _validate_specifiers(self, expectation_lines):
        errors = []
        for el in expectation_lines:
            for s in _SPECIFIER_GROUPS:
                if len(s.intersection(el.specifiers)) > 2:
                    errors.append('Expectation line contain more than one exclusive '
                                  'specifiers: %s (%s:%s). Please split this test '
                                  'expectation into multiple lines, each has one specifier.' % (
                        el.original_string, el.filename, el.line_numbers))
                    break
        if errors:
            raise ParseError(errors)

    def _create_expectation_line(self, test_name, expectations, file_name):
        expectation_line = TestExpectationLine()
        expectation_line.original_string = test_name
        expectation_line.name = test_name
        expectation_line.filename = file_name
        expectation_line.expectations = expectations
        return expectation_line

    def expectation_line_for_test(self, test_name, expectations):
        expectation_line = self._create_expectation_line(test_name, expectations, '<Bot TestExpectations>')
        self._parse_line(expectation_line)
        return expectation_line

    def expectation_for_skipped_test(self, test_name):
        if not self._port.test_exists(test_name):
            _log.warning('The following test %s from the Skipped list doesn\'t exist', test_name)
        expectation_line = self._create_expectation_line(test_name, [TestExpectationParser.PASS_EXPECTATION], '<Skipped file>')
        expectation_line.expectations = [TestExpectationParser.SKIP_MODIFIER]
        expectation_line.is_extra_skipped_test = True
        self._parse_line(expectation_line)
        return expectation_line

    def _parse_line(self, expectation_line):
        if not expectation_line.name:
            return

        if not self._check_test_exists(expectation_line):
            return

        expectation_line.is_file = self._port.test_isfile(expectation_line.name)
        if expectation_line.is_file:
            expectation_line.path = expectation_line.name
        else:
            expectation_line.path = self._port.normalize_test_name(expectation_line.name)

        self._collect_matching_tests(expectation_line)

        self._parse_specifiers(expectation_line)
        self._parse_expectations(expectation_line)

    def _parse_specifier(self, specifier):
        return specifier.lower()

    def _parse_specifiers(self, expectation_line):
        if self._is_lint_mode:
            self._lint_line(expectation_line)

        parsed_specifiers = set([self._parse_specifier(specifier) for specifier in expectation_line.specifiers])
        expectation_line.matching_configurations = self._test_configuration_converter.to_config_set(
            parsed_specifiers, expectation_line.warnings)

    def _lint_line(self, expectation_line):
        expectations = [expectation.lower() for expectation in expectation_line.expectations]
        specifiers = [specifier.lower() for specifier in expectation_line.specifiers]

    def _parse_expectations(self, expectation_line):
        result = set()
        for part in expectation_line.expectations:
            expectation = TestExpectations.expectation_from_string(part)
            if expectation is None:  # Careful, PASS is currently 0.
                expectation_line.warnings.append('Unsupported expectation: %s' % part)
                continue
            result.add(expectation)
        expectation_line.parsed_expectations = result

    def _check_test_exists(self, expectation_line):
        # WebKit's way of skipping tests is to add a -disabled suffix.
        # So we should consider the path existing if the path or the
        # -disabled version exists.
        if not self._port.test_exists(expectation_line.name) and not self._port.test_exists(expectation_line.name + '-disabled'):
            # Log a warning here since you hit this case any
            # time you update TestExpectations without syncing
            # the web_tests directory
            expectation_line.warnings.append('Path does not exist.')
            return False
        return True

    def _collect_matching_tests(self, expectation_line):
        """Convert the test specification to an absolute, normalized
        path and make sure directories end with the OS path separator.
        """
        if not self._all_tests:
            expectation_line.matching_tests = [expectation_line.path]
            return

        expectation_line.matching_tests = self._all_tests.find_matching_tests(expectation_line.path)

class TestExpectationLine(object):
    """Represents a line in test expectations file."""

    def __init__(self):
        """Initializes a blank-line equivalent of an expectation."""
        self.original_string = None
        self.filename = None  # this is the path to the expectations file for this line
        self.line_numbers = '0'
        self.name = None  # this is the path in the line itself
        self.path = None  # this is the normpath of self.name
        self.bugs = []
        self.specifiers = []
        self.parsed_specifiers = []
        self.matching_configurations = set()
        self.expectations = []
        self.parsed_expectations = set()
        self.comment = None
        self.matching_tests = []
        self.warnings = []
        self.is_extra_skipped_test = False

        # WARNING: If flag_expectations are not set, base_expectations will
        # always be empty. In this case, 'expectations' are base_expectations.
        # This quirk is caused by the way expectations are computed.
        #  base | flags | expectations | base_expectations | flag_expectations
        #   0   |   0   |   []         |        []         | []
        #   X   |   0   |   [base]     |        []         | []
        #   0   |   X   |   [flag]     |        [base]     | [flag]
        #   X   |   X   | [flag+base]  |        [base]     | [flag]
        self.base_expectations = []
        self.flag_expectations = []

    def __str__(self):
        return 'TestExpectationLine{name=%s, matching_configurations=%s, original_string=%s}' % (
            self.name, self.matching_configurations, self.original_string)

    def __eq__(self, other):
        return (isinstance(other, self.__class__)
                and self.original_string == other.original_string
                and self.filename == other.filename
                and self.line_numbers == other.line_numbers
                and self.name == other.name
                and self.path == other.path
                and self.bugs == other.bugs
                and self.specifiers == other.specifiers
                and self.parsed_specifiers == other.parsed_specifiers
                and self.matching_configurations == other.matching_configurations
                and self.expectations == other.expectations
                and self.parsed_expectations == other.parsed_expectations
                and self.comment == other.comment
                and self.matching_tests == other.matching_tests
                and self.warnings == other.warnings
                and self.is_extra_skipped_test == other.is_extra_skipped_test)

    def is_invalid(self):
        return bool(self.warnings and self.warnings != [TestExpectationParser.MISSING_BUG_WARNING])

    def is_flaky(self):
        return len(self.parsed_expectations) > 1

    def is_comment(self):
        return bool(re.match(r"^\s*#.*$", self.original_string))

    def is_whitespace(self):
        return not self.original_string.strip()

    # FIXME: Update the original specifiers and remove this once the old syntax is gone.
    _configuration_tokens_list = _PLATFORM_TOKENS_LIST + _BUILD_TYPE_TOKEN_LIST

    _configuration_tokens = dict((token, token.upper()) for token in _configuration_tokens_list)
    _inverted_configuration_tokens = dict((value, name) for name, value in _configuration_tokens.iteritems())

    # FIXME: Update the original specifiers list and remove this once the old syntax is gone.
    _expectation_tokens = {
        'Crash': 'CRASH',
        'Failure': 'FAIL',
        'Pass': 'PASS',
        'Skip': 'SKIP',
        'Slow': 'SLOW',
        'Timeout': 'TIMEOUT'
    }

    inverted_expectation_tokens = dict(
        [(value, name) for name, value in _expectation_tokens.items()])

    @classmethod
    def tokenize_line(cls, filename, expectation_string, line_number, port):
        """Tokenizes a line from TestExpectations and returns an unparsed TestExpectationLine instance using the old format.

        The new format for a test expectation line is:

        [[bugs] [ "[" <configuration specifiers> "]" <name> [ "[" <expectations> "]" ["#" <comment>]

        Any errant whitespace is not preserved.
        """
        expectation_line = TestExpectationLine()
        expectation_line.original_string = expectation_string
        expectation_line.filename = filename
        expectation_line.line_numbers = str(line_number)

        comment_index = expectation_string.find('#')
        if comment_index == -1:
            comment_index = len(expectation_string)
        else:
            expectation_line.comment = expectation_string[comment_index + 1:]

        remaining_string = re.sub(r"\s+", ' ', expectation_string[:comment_index].strip())
        if len(remaining_string) == 0:
            return expectation_line

        # special-case parsing this so that we fail immediately instead of treating this as a test name
        if remaining_string.startswith('//'):
            expectation_line.warnings = ['use "#" instead of "//" for comments']
            return expectation_line

        bugs = []
        specifiers = []
        name = None
        expectations = []
        warnings = []
        has_unrecognized_expectation = False

        tokens = remaining_string.split()
        state = 'start'
        for token in tokens:
            if (token.startswith(WEBKIT_BUG_PREFIX) or
                    token.startswith(CHROMIUM_BUG_PREFIX) or
                    token.startswith(SKIA_BUG_PREFIX) or
                    token.startswith(V8_BUG_PREFIX) or
                    token.startswith(NAMED_BUG_PREFIX)):
                if state != 'start':
                    warnings.append('"%s" is not at the start of the line.' % token)
                    break
                if token.startswith(WEBKIT_BUG_PREFIX):
                    bugs.append(token)
                elif token.startswith(CHROMIUM_BUG_PREFIX):
                    bugs.append(token)
                elif token.startswith(SKIA_BUG_PREFIX):
                    bugs.append(token)
                elif token.startswith(V8_BUG_PREFIX):
                    bugs.append(token)
                else:
                    match = re.match(r'Bug\((\w+)\)$', token)
                    if not match:
                        warnings.append('unrecognized bug identifier "%s"' % token)
                        break
                    else:
                        bugs.append(token)
            elif token == '[':
                if state == 'start':
                    state = 'configuration'
                elif state == 'name_found':
                    state = 'expectations'
                else:
                    warnings.append('unexpected "["')
                    break
            elif token == ']':
                if state == 'configuration':
                    state = 'name'
                elif state == 'expectations':
                    state = 'done'
                else:
                    warnings.append('unexpected "]"')
                    break
            elif token in ('//', ':', '='):
                warnings.append('"%s" is not legal in the new TestExpectations syntax.' % token)
                break
            elif state == 'configuration':
                if token not in cls._configuration_tokens:
                    warnings.append('Unrecognized specifier "%s"' % token)
                else:
                    specifiers.append(cls._configuration_tokens.get(token, token))
            elif state == 'expectations':
                if token not in cls._expectation_tokens:
                    has_unrecognized_expectation = True
                    warnings.append('Unrecognized expectation "%s"' % token)
                else:
                    expectations.append(cls._expectation_tokens.get(token, token))
            elif state == 'name_found':
                warnings.append('expecting "[", "#", or end of line instead of "%s"' % token)
                break
            else:
                name = token
                state = 'name_found'

        if not warnings:
            if not name:
                warnings.append('Did not find a test name.')
            elif state not in ('name_found', 'done'):
                warnings.append('Missing a "]"')

        if 'SKIP' in expectations and len(set(expectations) - {'SKIP'}):
            warnings.append('A test marked SKIP must not have other expectations.')

        if 'SLOW' in expectations and 'SlowTests' not in filename:
            warnings.append('SLOW tests should only be added to SlowTests and not to TestExpectations.')

        if 'NeverFixTests' in filename and expectations != ['SKIP']:
            warnings.append('Only SKIP expectations are allowed in NeverFixTests.')

        if 'SlowTests' in filename and port.is_wpt_test(name):
            warnings.append(
                'WPT should not be added to SlowTests; they should be marked as '
                'slow inside the test (see https://web-platform-tests.org/writing-tests/testharness-api.html#harness-timeout)')

        if 'SlowTests' in filename and expectations != ['SLOW']:
            warnings.append('Only SLOW expectations are allowed in SlowTests')

        if not expectations and not has_unrecognized_expectation:
            warnings.append('Missing expectations.')

        if 'MISSING' in expectations:
            warnings.append(
                '"Missing" expectations are not allowed; download new baselines '
                '(see https://goo.gl/SHVYrZ), or as a fallback, use "SKIP".')

        expectation_line.bugs = bugs
        expectation_line.specifiers = specifiers
        expectation_line.expectations = expectations
        expectation_line.name = name
        expectation_line.warnings = warnings
        return expectation_line

    @staticmethod
    def create_passing_expectation(test):
        expectation_line = TestExpectationLine()
        expectation_line.name = test
        expectation_line.path = test
        expectation_line.parsed_expectations = set([PASS])
        expectation_line.expectations = set(['PASS'])
        expectation_line.matching_tests = [test]
        return expectation_line

    @staticmethod
    def merge_expectation_lines(line1, line2, model_all_expectations):
        """Merges the expectations of line2 into line1 and returns a fresh object."""
        if line1 is None:
            return line2
        if line2 is None:
            return line1
        if model_all_expectations and line1.filename != line2.filename:
            return line2

        # Don't merge original_string or comment.
        result = TestExpectationLine()
        # We only care about filenames when we're linting, in which case the filenames are the same.
        # Not clear that there's anything better to do when not linting and the filenames are different.
        if model_all_expectations:
            result.filename = line2.filename
        result.line_numbers = line1.line_numbers + ',' + line2.line_numbers
        result.name = line1.name
        result.path = line1.path
        result.parsed_expectations = set(line1.parsed_expectations) | set(line2.parsed_expectations)
        result.expectations = list(set(line1.expectations) | set(line2.expectations))
        result.bugs = list(set(line1.bugs) | set(line2.bugs))
        result.specifiers = list(set(line1.specifiers) | set(line2.specifiers))
        result.parsed_specifiers = list(set(line1.parsed_specifiers) | set(line2.parsed_specifiers))
        result.matching_configurations = set(line1.matching_configurations) | set(line2.matching_configurations)
        result.matching_tests = list(list(set(line1.matching_tests) | set(line2.matching_tests)))
        result.warnings = list(set(line1.warnings) | set(line2.warnings))
        result.is_extra_skipped_test = line1.is_extra_skipped_test or line2.is_extra_skipped_test
        result.base_expectations = line1.base_expectations if line1.base_expectations else line2.base_expectations
        result.flag_expectations = line1.flag_expectations if line1.flag_expectations else line2.flag_expectations
        return result

    def to_string(self, test_configuration_converter=None, include_specifiers=True,
                  include_expectations=True, include_comment=True):
        parsed_expectation_to_string = dict(
            [[parsed_expectation, expectation_string]
             for expectation_string, parsed_expectation in TestExpectations.EXPECTATIONS.items()])

        if self.is_invalid():
            return self.original_string or ''

        if self.name is None:
            return '' if self.comment is None else '#%s' % self.comment

        if test_configuration_converter and self.bugs:
            specifiers_list = test_configuration_converter.to_specifiers_list(self.matching_configurations)
            result = []
            for specifiers in specifiers_list:
                # FIXME: this is silly that we join the specifiers and then immediately split them.
                specifiers = self._serialize_parsed_specifiers(test_configuration_converter, specifiers).split()
                expectations = self._serialize_parsed_expectations(parsed_expectation_to_string).split()
                result.append(self._format_line(self.bugs, specifiers, self.name, expectations, self.comment))
            return '\n'.join(result) if result else None

        return self._format_line(self.bugs, self.specifiers, self.name, self.expectations, self.comment,
                                 include_specifiers, include_expectations, include_comment)

    def to_csv(self):
        # Note that this doesn't include the comments.
        return '%s,%s,%s,%s' % (self.name, ' '.join(self.bugs), ' '.join(self.specifiers), ' '.join(self.expectations))

    def _serialize_parsed_expectations(self, parsed_expectation_to_string):
        result = []
        for index in TestExpectations.EXPECTATIONS.values():
            if index in self.parsed_expectations:
                result.append(parsed_expectation_to_string[index])
        return ' '.join(result)

    def _serialize_parsed_specifiers(self, test_configuration_converter, specifiers):
        result = []
        result.extend(sorted(self.parsed_specifiers))
        result.extend(test_configuration_converter.specifier_sorter().sort_specifiers(specifiers))
        return ' '.join(result)

    @staticmethod
    def _filter_redundant_expectations(expectations):
        if set(expectations) == set(['Pass', 'Skip']):
            return ['Skip']
        if set(expectations) == set(['Pass', 'Slow']):
            return ['Slow']
        if set(expectations) == set(['WontFix', 'Skip']):
            return ['WontFix']
        return expectations

    @classmethod
    def _format_line(cls, bugs, specifiers, name, expectations, comment, include_specifiers=True,
                     include_expectations=True, include_comment=True):
        new_specifiers = []
        new_expectations = []
        for specifier in specifiers:
            # FIXME: Make this all work with the mixed-cased specifiers (e.g. WontFix, Slow, etc).
            specifier = specifier.upper()
            new_specifiers.append(cls._inverted_configuration_tokens.get(specifier, specifier))

        for expectation in expectations:
            expectation = expectation.upper()
            new_expectations.append(cls.inverted_expectation_tokens.get(expectation, expectation))

        result = ''
        if include_specifiers and (bugs or new_specifiers):
            if bugs:
                result += ' '.join(bugs) + ' '
            if new_specifiers:
                result += '[ %s ] ' % ' '.join(new_specifiers)
        result += name
        if include_expectations and new_expectations:
            new_expectations = TestExpectationLine._filter_redundant_expectations(new_expectations)
            result += ' [ %s ]' % ' '.join(sorted(set(new_expectations)))
        if include_comment and comment is not None:
            result += ' #%s' % comment
        return result


# FIXME: Refactor API to be a proper CRUD.
class TestExpectationsModel(object):
    """Represents relational store of all expectations and provides CRUD semantics to manage it."""

    def __init__(self, shorten_filename=None):
        # Maps a test to its list of expectations.
        self._test_to_expectations = {}

        # Maps a test to list of its specifiers (string values)
        self._test_to_specifiers = {}

        # Maps a test to a TestExpectationLine instance.
        self._test_to_expectation_line = {}

        self._expectation_to_tests = self._dict_of_sets(TestExpectations.EXPECTATIONS)
        self._result_type_to_tests = self._dict_of_sets(TestExpectations.RESULT_TYPES)

        self._shorten_filename = shorten_filename or (lambda x: x)
        self._flag_name = None

    def all_lines(self):
        return sorted(self._test_to_expectation_line.values(),
                      cmp=self._compare_lines)

    def _compare_lines(self, line_a, line_b):
        if line_a.name == line_b.name:
            return 0
        if line_a.name < line_b.name:
            return -1
        return 1

    def _merge_test_map(self, self_map, other_map):
        for test in other_map:
            new_expectations = set(other_map[test])
            if test in self_map:
                new_expectations |= set(self_map[test])
            self_map[test] = list(new_expectations) if isinstance(other_map[test], list) else new_expectations

    def _merge_dict_of_sets(self, self_dict, other_dict):
        for key in other_dict:
            self_dict[key] |= other_dict[key]

    def merge_model(self, other, is_flag_specific=False):
        self._merge_test_map(self._test_to_expectations, other._test_to_expectations)

        # merge_expectation_lines is O(tests per line). Therefore, this loop
        # is O((tests per line)^2) which is really expensive when a line
        # contains a lot of tests. Cache the output of merge_expectation_lines
        # so that we only call that n^2 in the number of *lines*.
        merge_lines_cache = defaultdict(dict)

        for test, other_line in other._test_to_expectation_line.items():
            merged_line = None
            if test in self._test_to_expectation_line:
                self_line = self._test_to_expectation_line[test]

                if other_line not in merge_lines_cache[self_line]:
                    merge_lines_cache[self_line][other_line] = TestExpectationLine.merge_expectation_lines(
                        self_line, other_line, model_all_expectations=False)
                    if is_flag_specific:
                        merge_lines_cache[self_line][other_line].base_expectations = self_line.expectations
                        merge_lines_cache[self_line][other_line].flag_expectations = other_line.expectations

                merged_line = merge_lines_cache[self_line][other_line]
            else:
                merged_line = other_line
                if is_flag_specific:
                    merged_line.flag_expectations = other_line.expectations

            self._test_to_expectation_line[test] = merged_line

        self._merge_dict_of_sets(self._expectation_to_tests, other._expectation_to_tests)
        self._merge_dict_of_sets(self._result_type_to_tests, other._result_type_to_tests)

    def _dict_of_sets(self, strings_to_constants):
        """Takes a dictionary of keys to values and returns a dict mapping each value to an empty set."""
        result = {}
        for value in strings_to_constants.values():
            result[value] = set()
        return result

    def get_test_set(self, expectation, include_skips=True):
        tests = self._expectation_to_tests[expectation]
        if not include_skips:
            tests = tests - self.get_test_set(SKIP)
        return tests

    def get_test_set_for_keyword(self, keyword):
        expectation_enum = TestExpectations.EXPECTATIONS.get(keyword.lower(), None)
        if expectation_enum is not None:
            return self._expectation_to_tests[expectation_enum]

        matching_tests = set()
        for test, specifiers in self._test_to_specifiers.iteritems():
            if keyword.lower() in specifiers:
                matching_tests.add(test)
        return matching_tests

    def get_tests_with_result_type(self, result_type):
        return self._result_type_to_tests[result_type]

    def has_test(self, test):
        return test in self._test_to_expectation_line

    def get_flag_name(self):
        return self._flag_name

    def append_flag_name(self, flag_name):
        if self._flag_name:
            self._flag_name += ' ' + flag_name
        else:
            self._flag_name = flag_name

    def get_expectation_line(self, test):
        return self._test_to_expectation_line.get(test)

    def get_expectations(self, test):
        return self._test_to_expectations[test]

    def get_expectations_string(self, test):
        """Returns the expectations for the given test as an uppercase string.
        If there are no expectations for the test, KeyError is raised.
        """
        if self.get_expectation_line(test).is_extra_skipped_test:
            return 'NOTRUN'

        expectations = self.get_expectations(test)
        retval = []

        for expectation in expectations:
            retval.append(TestExpectations.expectation_to_string(expectation))

        return ' '.join(retval)

    def add_expectation_line(self, expectation_line,
                             model_all_expectations=False):
        """Returns a list of warnings encountered while matching specifiers."""

        if expectation_line.is_invalid():
            return

        for test in expectation_line.matching_tests:
            if self._already_seen_better_match(test, expectation_line):
                continue

            if model_all_expectations:
                expectation_line = TestExpectationLine.merge_expectation_lines(
                    self.get_expectation_line(test), expectation_line, model_all_expectations)

            self._clear_expectations_for_test(test)
            self._test_to_expectation_line[test] = expectation_line
            self._add_test(test, expectation_line)

    def _add_test(self, test, expectation_line):
        """Sets the expected state for a given test.

        This routine assumes the test has not been added before. If it has,
        use _clear_expectations_for_test() to reset the state prior to
        calling this.
        """
        self._test_to_expectations[test] = expectation_line.parsed_expectations
        for expectation in expectation_line.parsed_expectations:
            self._expectation_to_tests[expectation].add(test)

        self._test_to_specifiers[test] = expectation_line.specifiers

        if SKIP in expectation_line.parsed_expectations:
            self._result_type_to_tests[SKIP].add(test)
        elif TIMEOUT in expectation_line.parsed_expectations:
            self._result_type_to_tests[TIMEOUT].add(test)
        elif expectation_line.parsed_expectations == set([PASS]):
            self._result_type_to_tests[PASS].add(test)
        elif expectation_line.is_flaky():
            self._result_type_to_tests[FLAKY].add(test)
        else:
            # FIXME: What is this?
            self._result_type_to_tests[FAIL].add(test)

    def _clear_expectations_for_test(self, test):
        """Remove preexisting expectations for this test.
        This happens if we are seeing a more precise path
        than a previous listing.
        """
        if self.has_test(test):
            self._test_to_expectations.pop(test, '')
            self._remove_from_sets(test, self._expectation_to_tests)
            self._remove_from_sets(test, self._result_type_to_tests)

    def _remove_from_sets(self, test, dict_of_sets_of_tests):
        """Removes the given test from the sets in the dictionary.

        Args:
          test: test to look for
          dict: dict of sets of files
        """
        for set_of_tests in dict_of_sets_of_tests.itervalues():
            if test in set_of_tests:
                set_of_tests.remove(test)

    def _already_seen_better_match(self, test, expectation_line):
        """Returns whether we've seen a better match already in the file.

        Returns True if we've already seen a expectation_line.name that matches more of the test
            than this path does
        """
        # FIXME: See comment below about matching test configs and specificity.
        if not self.has_test(test):
            # We've never seen this test before.
            return False

        prev_expectation_line = self._test_to_expectation_line[test]

        if prev_expectation_line.filename != expectation_line.filename:
            # We've moved on to a new expectation file, which overrides older ones.
            return False

        if len(prev_expectation_line.path) > len(expectation_line.path):
            # The previous path matched more of the test.
            return True

        if len(prev_expectation_line.path) < len(expectation_line.path):
            # This path matches more of the test.
            return False

        # At this point we know we have seen a previous exact match on this
        # base path, so we need to check the two sets of specifiers.

        # FIXME: This code was originally designed to allow lines that matched
        # more specifiers to override lines that matched fewer specifiers.
        # However, we currently view these as errors.
        #
        # To use the "more specifiers wins" policy, change the errors for overrides
        # to be warnings and return False".

        if prev_expectation_line.matching_configurations == expectation_line.matching_configurations:
            expectation_line.warnings.append('Duplicate or ambiguous entry lines %s:%s and %s:%s.' % (
                self._shorten_filename(prev_expectation_line.filename), prev_expectation_line.line_numbers,
                self._shorten_filename(expectation_line.filename), expectation_line.line_numbers))
            return True

        if prev_expectation_line.matching_configurations >= expectation_line.matching_configurations:
            expectation_line.warnings.append('More specific entry for %s on line %s:%s overrides line %s:%s.' % (
                expectation_line.name,
                self._shorten_filename(
                    prev_expectation_line.filename), prev_expectation_line.line_numbers,
                self._shorten_filename(expectation_line.filename), expectation_line.line_numbers))
            # FIXME: return False if we want more specific to win.
            return True

        if prev_expectation_line.matching_configurations <= expectation_line.matching_configurations:
            expectation_line.warnings.append('More specific entry for %s on line %s:%s overrides line %s:%s.' % (
                expectation_line.name,
                self._shorten_filename(
                    expectation_line.filename), expectation_line.line_numbers,
                self._shorten_filename(prev_expectation_line.filename), prev_expectation_line.line_numbers))
            return True

        if prev_expectation_line.matching_configurations & expectation_line.matching_configurations:
            expectation_line.warnings.append('Entries for %s on lines %s:%s and %s:%s match overlapping sets of configurations.' % (
                expectation_line.name,
                self._shorten_filename(
                    prev_expectation_line.filename), prev_expectation_line.line_numbers,
                self._shorten_filename(expectation_line.filename), expectation_line.line_numbers))
            return True

        # Configuration sets are disjoint, then.
        return False


class TestExpectations(object):
    """Test expectations consist of lines with specifications of what
    to expect from web test cases. The test cases can be directories
    in which case the expectations apply to all test cases in that
    directory and any subdirectory. The format is along the lines of:

      fast/js/fixme.js [ Failure ]
      fast/js/flaky.js [ Failure Pass ]
      fast/js/crash.js [ Crash Failure Pass Timeout ]
      ...

    To add specifiers:
      fast/js/no-good.js
      [ Debug ] fast/js/no-good.js [ Pass Timeout ]
      [ Debug ] fast/js/no-good.js [ Pass Skip Timeout ]
      [ Linux Debug ] fast/js/no-good.js [ Pass Skip Timeout ]
      [ Linux Win ] fast/js/no-good.js [ Pass Skip Timeout ]

    Skip: Doesn't run the test.
    Slow: The test takes a long time to run, but does not timeout indefinitely.
    WontFix: For tests that we never intend to pass on a given platform (treated like Skip).

    Notes:
      -A test cannot be both SLOW and TIMEOUT
      -A test can be included twice, but not via the same path.
      -If a test is included twice, then the more precise path wins.
    """

    # FIXME: Update to new syntax once the old format is no longer supported.
    EXPECTATIONS = {
        'pass': PASS,
        'fail': FAIL,
        'timeout': TIMEOUT,
        'crash': CRASH,
        TestExpectationParser.SKIP_MODIFIER: SKIP,
        TestExpectationParser.SLOW_MODIFIER: SLOW,
    }

    EXPECTATIONS_TO_STRING = {k: v.upper() for (v, k) in EXPECTATIONS.iteritems()}

    # (aggregated by category, pass/fail/skip, type)
    EXPECTATION_DESCRIPTIONS = {
        SKIP: 'skipped',
        PASS: 'passes',
        FAIL: 'failures',
        CRASH: 'crashes',
        TIMEOUT: 'timeouts',
    }

    BUILD_TYPES = ('debug', 'release')

    RESULT_TYPES = {
        'skip': SKIP,
        'pass': PASS,
        'fail': FAIL,
        'flaky': FLAKY,
        'timeout': TIMEOUT,
    }

    @classmethod
    def expectation_from_string(cls, string):
        assert ' ' not in string  # This only handles one expectation at a time.
        return cls.EXPECTATIONS.get(string.lower())

    @classmethod
    def expectation_to_string(cls, expectation):
        """Return the uppercased string equivalent of a given expectation."""
        try:
            return cls.EXPECTATIONS_TO_STRING[expectation]
        except KeyError:
            raise ValueError(expectation)

    @staticmethod
    def result_was_expected(result, expected_results):
        """Returns whether we got a result we were expecting.
        Args:
            result: actual result of a test execution
            expected_results: set of results listed in test_expectations
        """
        local_expected = set(expected_results)

        # Make sure we have at least one result type that may actually happen.
        local_expected.discard(SLOW)
        if not local_expected:
            local_expected = {PASS}

        if result in local_expected:
            return True
        return False

    # FIXME: This constructor does too much work. We should move the actual parsing of
    # the expectations into separate routines so that linting and handling overrides
    # can be controlled separately, and the constructor can be more of a no-op.
    def __init__(self, port, tests=None, include_overrides=True, expectations_dict=None,
                 model_all_expectations=False, is_lint_mode=False):
        self._full_test_list = tests
        self._test_config = port.test_configuration()
        self._is_lint_mode = is_lint_mode
        self._model_all_expectations = self._is_lint_mode or model_all_expectations
        self._model = TestExpectationsModel(self._shorten_filename)
        self._parser = TestExpectationParser(port, tests, self._is_lint_mode)
        self._port = port
        self._skipped_tests_warnings = []
        self._expectations = []

        if not expectations_dict:
            expectations_dict = port.expectations_dict()

        if expectations_dict:
            # Always parse the generic expectations (the generic file is required
            # to be the first one in the expectations_dict, which must be an OrderedDict).
            generic_path, generic_exps = expectations_dict.items()[0]
            expectations = self._parser.parse(generic_path, generic_exps)
            self._add_expectations(expectations, self._model)
            self._expectations += expectations

            # Now add the overrides if so requested.
            if include_overrides:
                for path, contents in expectations_dict.items()[1:]:
                    expectations = self._parser.parse(path, contents)
                    model = TestExpectationsModel(self._shorten_filename)
                    self._add_expectations(expectations, model)
                    self._expectations += expectations
                    flag_specific_match = re.match('.*' + port.FLAG_EXPECTATIONS_PREFIX + '(.*)', path)
                    if flag_specific_match is not None:
                        self._model.append_flag_name(flag_specific_match.group(1))
                    self._model.merge_model(model, flag_specific_match is not None)

        self.add_extra_skipped_tests(set(port.get_option('ignore_tests', [])))
        self.add_expectations_from_bot()

        self._has_warnings = False
        self._report_warnings()
        self._process_tests_without_expectations()

    # TODO(ojan): Allow for removing skipped tests when getting the list of
    # tests to run, but not when getting metrics.
    def model(self):
        return self._model

    def expectations(self):
        return self._expectations

    # FIXME: Change the callsites to use TestExpectationsModel and remove.
    def get_expectations(self, test):
        return self._model.get_expectations(test)

    # FIXME: Change the callsites to use TestExpectationsModel and remove.
    def get_tests_with_result_type(self, result_type):
        return self._model.get_tests_with_result_type(result_type)

    # FIXME: Change the callsites to use TestExpectationsModel and remove.
    def get_test_set(self, expectation, include_skips=True):
        return self._model.get_test_set(expectation, include_skips)

    def get_expectations_string(self, test):
        return self._model.get_expectations_string(test)

    def matches_an_expected_result(self, test, result):
        expected_results = self._model.get_expectations(test)
        return self.result_was_expected(result, expected_results)

    def _shorten_filename(self, filename):
        finder = PathFinder(self._port.host.filesystem)
        if filename.startswith(finder.path_from_chromium_base()):
            return self._port.host.filesystem.relpath(filename, finder.path_from_chromium_base())
        return filename

    def _report_warnings(self):
        warnings = []
        for expectation in self._expectations:
            for warning in expectation.warnings:
                warnings.append('%s:%s %s %s' % (
                    self._shorten_filename(expectation.filename), expectation.line_numbers,
                    warning, expectation.name if expectation.expectations else expectation.original_string))

        if warnings:
            self._has_warnings = True
            if self._is_lint_mode:
                raise ParseError(warnings)
            _log.warning('--lint-test-files warnings:')
            for warning in warnings:
                _log.warning(warning)
            _log.warning('')

    def _process_tests_without_expectations(self):
        if self._full_test_list:
            for test in self._full_test_list:
                if not self._model.has_test(test):
                    self._model.add_expectation_line(TestExpectationLine.create_passing_expectation(test))

    def has_warnings(self):
        return self._has_warnings

    def remove_configurations(self, removals):
        expectations_to_remove = []
        modified_expectations = []

        for test, test_configuration in removals:
            for expectation in self._expectations:
                if expectation.name != test or not expectation.parsed_expectations:
                    continue
                if test_configuration not in expectation.matching_configurations:
                    continue

                expectation.matching_configurations.remove(test_configuration)
                if expectation.matching_configurations:
                    modified_expectations.append(expectation)
                else:
                    expectations_to_remove.append(expectation)

        for expectation in expectations_to_remove:
            index = self._expectations.index(expectation)
            self._expectations.remove(expectation)

            if index == len(self._expectations) or self._expectations[
                    index].is_whitespace() or self._expectations[index].is_comment():
                while index and self._expectations[index - 1].is_comment():
                    index = index - 1
                    self._expectations.pop(index)
                while index and self._expectations[index - 1].is_whitespace():
                    index = index - 1
                    self._expectations.pop(index)

        return self.list_to_string(self._expectations, self._parser._test_configuration_converter, modified_expectations)

    def _add_expectations(self, expectation_list, model):
        for expectation_line in expectation_list:
            if not expectation_line.expectations:
                continue

            if self._model_all_expectations or self._test_config in expectation_line.matching_configurations:
                model.add_expectation_line(expectation_line, model_all_expectations=self._model_all_expectations)

    def add_extra_skipped_tests(self, tests_to_skip):
        if not tests_to_skip:
            return
        for test in self._expectations:
            if test.name and test.name in tests_to_skip:
                test.warnings.append('%s:%s %s is also in a Skipped file.' % (test.filename, test.line_numbers, test.name))

        model = TestExpectationsModel(self._shorten_filename)
        for test_name in tests_to_skip:
            expectation_line = self._parser.expectation_for_skipped_test(test_name)
            model.add_expectation_line(expectation_line)
        self._model.merge_model(model)

    def add_expectations_from_bot(self):
        # FIXME: With mode 'very-flaky' and 'maybe-flaky', this will show the expectations entry in the flakiness
        # dashboard rows for each test to be whatever the bot thinks they should be. Is this a good thing?
        bot_expectations = self._port.bot_expectations()
        model = TestExpectationsModel(self._shorten_filename)
        for test_name in bot_expectations:
            expectation_line = self._parser.expectation_line_for_test(test_name, bot_expectations[test_name])

            # Unexpected results are merged into existing expectations.
            model.add_expectation_line(expectation_line)
        self._model.merge_model(model)

    def add_expectation_line(self, expectation_line):
        self._model.add_expectation_line(expectation_line)
        self._expectations += [expectation_line]

    @staticmethod
    def list_to_string(expectation_lines, test_configuration_converter=None, reconstitute_only_these=None):
        def serialize(expectation_line):
            # If reconstitute_only_these is an empty list, we want to return original_string.
            # So we need to compare reconstitute_only_these to None, not just check if it's falsey.
            if reconstitute_only_these is None or expectation_line in reconstitute_only_these:
                return expectation_line.to_string(test_configuration_converter)
            return expectation_line.original_string

        def nones_out(expectation_line):
            return expectation_line is not None

        return '\n'.join(filter(nones_out, map(serialize, expectation_lines)))
