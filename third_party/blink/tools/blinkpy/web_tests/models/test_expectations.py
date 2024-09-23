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

import bisect
import copy
import logging
import re
from collections import defaultdict
from collections import OrderedDict
from dataclasses import dataclass, field
from functools import reduce
from typing import (
    ClassVar,
    Collection,
    Dict,
    FrozenSet,
    List,
    Mapping,
    Optional,
    Set,
    Tuple,
)

from blinkpy.common.memoized import memoized
from blinkpy.web_tests.models import typ_types
from typ import expectations_parser

ResultType = typ_types.ResultType

_log = logging.getLogger(__name__)

SPECIAL_PREFIXES = ('# tags:', '# results:', '# conflicts_allowed:')
EXPECTATION_DESCRIPTIONS = {
    ResultType.Skip: 'skipped',
    ResultType.Pass: 'passes',
    ResultType.Failure: 'failures',
    ResultType.Crash: 'crashes',
    ResultType.Timeout: 'timeouts',
}


class _NotExpectation(typ_types.ExpectationType):
    '''This class is a placeholder for emtpy lines or comments in the
    test expectations file. It has the same API as typ_types.Expectations.
    However the test member variable is set to an empty string and there
    are no expected results in this line.'''

    def __init__(self, line, lineno):
        super(_NotExpectation, self).__init__(test='', lineno=lineno)
        self._line = line

    def to_string(self):
        return self._line


class ParseError(Exception):
    def __init__(self, errors):
        self.errors = errors

    def __str__(self):
        return '\n'.join(self.errors)

    def __repr__(self):
        return 'ParseError(errors=%s)' % str(self.errors)


@dataclass
class ExpectationsChange:
    lines_added: List[typ_types.ExpectationType] = field(default_factory=list)
    lines_removed: List[typ_types.ExpectationType] = field(
        default_factory=list)

    def __add__(self, other: 'ExpectationsChange') -> 'ExpectationsChange':
        lines_added = {line.to_string(): line for line in self.lines_added}
        lines_removed = {line.to_string(): line for line in self.lines_removed}
        self._add_delta(other.lines_added, lines_removed, lines_added)
        self._add_delta(other.lines_removed, lines_added, lines_removed)
        return ExpectationsChange(list(lines_added.values()),
                                  list(lines_removed.values()))

    def _add_delta(self, lines: Collection[typ_types.ExpectationType],
                   negative: Dict[str, typ_types.ExpectationType],
                   positive: Dict[str, typ_types.ExpectationType]):
        for line in lines:
            formatted_line = line.to_string()
            if formatted_line in negative:
                negative.pop(formatted_line)
            else:
                positive[formatted_line] = line


def _exp_order(exp: typ_types.ExpectationType):
    formatted_line = exp.to_string().strip()
    meaningful = formatted_line and not formatted_line.startswith('#')
    # Format empty lines and comments before semantically meaningful content.
    return meaningful, formatted_line


class TestExpectations:
    def __init__(self, port, expectations_dict=None):
        self._port = port
        self._system_condition_tags = self._port.get_platform_tags()
        self._expectations = []
        self._expectations_dict = OrderedDict(
            expectations_dict or port.expectations_dict())
        filesystem = self._port.host.filesystem
        expectation_errors = []

        # Separate expectations from flag specific files into a base
        # expectations list and flag expectations list. We will also store
        # the blink flags in a list.
        self._flags = []
        self._flag_expectations = []
        self._base_expectations = []
        # map file paths to sets of line numbers
        self._expectation_file_linenos = defaultdict(set)

        for path, content in self._expectations_dict.items():
            test_expectations = typ_types.TestExpectations(
                tags=self._system_condition_tags)
            ret, errors = test_expectations.parse_tagged_list(
                content,
                file_name=filesystem.abspath(path),
                tags_conflict=self._tags_conflict)
            if ret:
                expectation_errors.append(
                    'Parsing file %s produced following errors\n%s' % (path,
                                                                       errors))
            self._expectations.append(test_expectations)
            flag_match = re.match(
                '.*' + port.FLAG_EXPECTATIONS_PREFIX + '(.*)', path)

            self._reset_lines(path)

            # If file is a flag specific file, store the typ.TestExpectation
            # instance in _flag_expectations list, otherwise store it in
            # _base_expectations
            if flag_match:
                self._flags.append(flag_match.group(1))
                self._flag_expectations.append(test_expectations)
            else:
                self._base_expectations.append(test_expectations)

        if port.get_option('ignore_tests', []):
            content = '# results: [ Skip ]\n'
            for pattern in set(port.get_option('ignore_tests', [])):
                if filesystem.isdir(
                        filesystem.join(self._port.web_tests_dir(), pattern)):
                    pattern += '*'
                content += '%s [ Skip ]\n' % pattern
            test_expectations = typ_types.TestExpectations()
            ret, errors = test_expectations.parse_tagged_list(content)
            if ret:
                expectation_errors.append(
                    'Parsing patterns passed through --ignore produced the following errors\n%s'
                    % errors)
            self._expectations.append(test_expectations)
        if expectation_errors:
            raise ParseError(expectation_errors)
        self._add_expectations_from_bot()

    def set_system_condition_tags(self, tags):
        for test_exps in self._expectations:
            test_exps.set_tags(tags)
        self._system_condition_tags = tags

    @staticmethod
    def _maybe_remove_comments_and_whitespace(lines):
        """If the last expectation in a block is deleted, then remove all associated
        comments and white spaces.

        args:
            lines: Array which contains Expectation instances for each line in an
                   expectations file."""
        # remove comments associated with deleted expectation
        while (lines and lines[-1].to_string().strip().startswith('#')
               and not any(lines[-1].to_string().strip().startswith(prefix)
                           for prefix in SPECIAL_PREFIXES)):
            lines.pop()

        # remove spaces above expectation
        while lines and lines[-1].to_string().strip() == '':
            lines.pop()

    def get_updated_lines(self, path):
        return copy.deepcopy(self._reset_lines(path))

    def _reset_lines(self, path):
        """This method returns the Expectation instances for each line
        in an expectations file. If there were any modifications made
        through the remove_expectations or add_expectations member methods
        then this method will update the in memory view of the expectations
        file.

        args:
            path: Absolute path of expectations file."""
        content = self._expectations_dict[path]
        idx = list(self._expectations_dict.keys()).index(path)
        typ_expectations = self._expectations[idx]
        lines = []

        # Store Expectation instances for each line
        lineno_to_exps = defaultdict(list)

        for pattern_to_exps in (typ_expectations.individual_exps,
                                typ_expectations.glob_exps):
            for test in sorted(pattern_to_exps):
                exps = pattern_to_exps[test]
                for exp in exps:
                    lineno_to_exps[exp.lineno].append(exp)

        removed_linenos = (self._expectation_file_linenos[path] -
                           set(lineno_to_exps.keys()))
        content_lines = content.splitlines()

        for lineno, line in enumerate(content_lines, 1):
            if not line.strip() or line.strip().startswith('#'):
                lines.append(_NotExpectation(line, lineno))
                if lineno in lineno_to_exps:
                    lines.extend(lineno_to_exps[lineno])
                    lineno_to_exps.pop(lineno)
            elif lineno in removed_linenos:
                next_line = ''
                if lineno < len(content_lines):
                    next_line = content_lines[lineno].strip()

                if not next_line or next_line.startswith('#'):
                    self._maybe_remove_comments_and_whitespace(lines)
            else:
                exps = lineno_to_exps[lineno]
                lines.extend(sorted(exps, key=_exp_order))
                lineno_to_exps.pop(lineno)

        # Handle Expectation instances with line numbers outside of the
        # [1, total file line count] range. There are two cases for
        # Expectation instances with line numbers outside the valid range.
        #
        # 1, If line number is 0 then the Expectation instance will be appended
        #    to the file.
        # 2, If the line number is greater than the total number of lines then
        #    an exception will be raised.
        if lineno_to_exps:
            lines.append(_NotExpectation('', len(content_lines) + 1))

            for line in sorted(reduce(lambda x, y: x + y,
                                      list(lineno_to_exps.values())),
                               key=lambda e: e.test):
                if line.lineno:
                    raise ValueError(
                        "Expectation '%s' was given a line number that "
                        "is greater than the total line count of file %s."
                        % (line.to_string(), path))
                lines.append(line)

        self._expectation_file_linenos[path] = {
            line.lineno for line in lines
            if not isinstance(line, _NotExpectation)}

        return lines

    def commit_changes(self):
        """Writes to the expectations files any modifications made
        through the remove_expectations or add_expectations member
        methods"""
        for path in self._expectations_dict:
            exp_lines = self._reset_lines(path)
            new_content = '\n'.join(
                [e.to_string() for e in exp_lines]) + '\n'

            self._expectations_dict[path] = new_content
            self._expectation_file_linenos[path] = set()

            for lineno, exp in enumerate(exp_lines, 1):
                exp.lineno = lineno
                if not isinstance(exp, _NotExpectation):
                    self._expectation_file_linenos[path].add(lineno)

            self._port.host.filesystem.write_text_file(path, new_content)

    @property
    def flag_name(self):
        return ' '.join(self._flags)

    @property
    def port(self):
        return self._port

    @property
    def expectations_dict(self):
        return self._expectations_dict

    @property
    def system_condition_tags(self):
        return self._system_condition_tags

    @memoized
    def _os_to_version(self):
        os_to_version = {}
        for os, os_versions in \
            self._port.configuration_specifier_macros().items():
            for version in os_versions:
                os_to_version[version.lower()] = os.lower()
        return os_to_version

    def _tags_conflict(self, t1, t2):
        os_to_version = self._os_to_version()
        if not t1 in os_to_version and not t2 in os_to_version:
            return t1 != t2
        elif t1 in os_to_version and t2 in os_to_version:
            return t1 != t2
        elif t1 in os_to_version:
            return os_to_version[t1] != t2
        else:
            return os_to_version[t2] != t1

    def merge_raw_expectations(self, content):
        test_expectations = typ_types.TestExpectations()
        test_expectations.parse_tagged_list(content)
        self._expectations.append(test_expectations)

    def _get_expectations(self, expectations, test, original_test=None):
        results = set()
        reasons = set()
        is_slow_test = False
        trailing_comments = ''
        for test_exp in expectations:
            expected_results = test_exp.expectations_for(test)
            # The return Expectation instance from expectations_for has the default
            # PASS expected result. If there are no expected results in the first
            # file and there are expected results in the second file, then the JSON
            # results will show an expected per test field with PASS and whatever the
            # expected results in the second file are.
            if not expected_results.is_default_pass:
                if expected_results.conflict_resolution == \
                        expectations_parser.ConflictResolutionTypes.OVERRIDE:
                    results.clear()
                    reasons.clear()
                    is_slow_test = False
                    trailing_comments = ''
                results.update(expected_results.results)
            is_slow_test |= expected_results.is_slow_test
            reasons.update(expected_results.reason.split())
            # Typ will leave a newline at the end of trailing_comments, so we
            # can just concatenate here and still have comments from different
            # files be separated by newlines.
            trailing_comments += expected_results.trailing_comments

        # If the results set is empty then the Expectation constructor
        # will set the expected result to Pass.
        return typ_types.Expectation(test=original_test or test,
                                     results=results,
                                     is_slow_test=is_slow_test,
                                     reason=' '.join(reasons),
                                     trailing_comments=trailing_comments)

    def get_expectations_from_file(self, path, test_name):
        idx = list(self._expectations_dict.keys()).index(path)
        return copy.deepcopy(
            self._expectations[idx].individual_exps.get(test_name) or [])

    @staticmethod
    def _override_or_fallback_expectations(override, fallback):
        if override.is_default_pass:
            fallback.is_slow_test |= override.is_slow_test
            return fallback
        override.is_slow_test |= fallback.is_slow_test
        return override

    def _get_expectations_with_fallback(self,
                                        expectations,
                                        fallback_expectations,
                                        test,
                                        original_test=None):
        exp = self._override_or_fallback_expectations(
            self._get_expectations(expectations, test, original_test),
            self._get_expectations(fallback_expectations, test, original_test))
        base_test = self.port.lookup_virtual_test_base(test)
        if base_test:
            return self._override_or_fallback_expectations(
                exp,
                self._get_expectations_with_fallback(expectations,
                                                     fallback_expectations,
                                                     base_test, test))
        return exp

    @memoized
    def get_expectations(self, test):
        return self._get_expectations_with_fallback(self._flag_expectations,
                                                    self._expectations, test)

    @memoized
    def get_flag_expectations(self, test):
        exp = self._get_expectations_with_fallback(self._flag_expectations, [],
                                                   test)
        if exp.is_default_pass:
            return None
        return exp

    @memoized
    def get_base_expectations(self, test):
        return self._get_expectations_with_fallback(self._base_expectations,
                                                    [], test)

    def get_tests_with_expected_result(self, result):
        """This method will return a list of tests and directories which
        have the result argument value in its expected results

        args:
            result: ResultType value, i.e ResultType.Skip"""
        tests = []
        for test_exp in self._expectations:
            tests.extend(test_exp.individual_exps)
            tests.extend([
                dir_name[:-1] for dir_name in test_exp.glob_exps.keys()
                if self.port.test_isdir(dir_name[:-1])
            ])
        return {
            test_name
            for test_name in tests
            if result in self.get_expectations(test_name).results
        }

    def matches_an_expected_result(self, test, result):
        expected_results = self.get_expectations(test).results
        return result in expected_results

    def _add_expectations_from_bot(self):
        # FIXME: With mode 'very-flaky' and 'maybe-flaky', this will show
        # the expectations entry in the flakiness dashboard rows for each
        # test to be whatever the bot thinks they should be. Is this a
        # good thing?
        bot_expectations = self._port.bot_expectations()
        if bot_expectations:
            raw_expectations = (
                '# results: [ Failure Pass Crash Skip Timeout ]\n')
            for test, results in bot_expectations.items():
                raw_expectations += typ_types.Expectation(
                    test=test, results=results).to_string() + '\n'
            self.merge_raw_expectations(raw_expectations)

    def remove_expectations(self, path, exps) -> ExpectationsChange:
        """This method removes Expectation instances from an expectations file.
        It will delete the line in the expectations file associated with the
        Expectation instance.

        args:
            path: Absolute path of file where the Expectation instances
                  came from.
            exps: List of Expectation instances to be deleted."""
        idx = list(self._expectations_dict.keys()).index(path)
        typ_expectations = self._expectations[idx]

        for exp in exps:
            if exp.is_glob:
                pattern_to_exps = typ_expectations.glob_exps
            else:
                pattern_to_exps = typ_expectations.individual_exps
            pattern_to_exps[exp.test].remove(exp)
            if not pattern_to_exps[exp.test]:
                pattern_to_exps.pop(exp.test)
        return ExpectationsChange(lines_removed=exps)

    def add_expectations(self,
                         path: str,
                         exps: List[typ_types.ExpectationType],
                         lineno: int = 0) -> ExpectationsChange:
        """This method adds Expectation instances to an expectations file. It will
        add the new instances after the line number passed through the lineno parameter.
        If the lineno is set to a value outside the range of line numbers in the file
        then it will append the expectations to the end of the file

        Arguments:
            path: Absolute path of file where expectations will be added to.
            exps: List of Expectation instances to be added to the file.
            lineno: Line number in expectations file where the expectations will
                be added. Provide 0 to append to the end of the file.
        """
        idx = list(self._expectations_dict.keys()).index(path)
        typ_expectations = self._expectations[idx]
        added_glob = False

        if lineno < 0:
            raise ValueError('lineno cannot be negative.')

        for exp in exps:
            exp.lineno = lineno

        for exp in exps:
            added_glob |= exp.is_glob
            if exp.is_glob:
                typ_expectations.glob_exps.setdefault(exp.test, []).append(exp)
            else:
                typ_expectations.individual_exps.setdefault(exp.test,
                                                            []).append(exp)

        if added_glob:
            glob_exps = reduce(lambda x, y: x + y,
                               list(typ_expectations.glob_exps.values()))
            glob_exps.sort(key=lambda e: len(e.test), reverse=True)
            typ_expectations.glob_exps = OrderedDict()
            for exp in glob_exps:
                typ_expectations.glob_exps.setdefault(exp.test, []).append(exp)
        return ExpectationsChange(lines_added=exps)


class SystemConfigurationEditor:
    ALL_SYSTEMS: ClassVar[str] = ''  # Sentinel value to indicate no tag

    def __init__(self,
                 test_expectations: TestExpectations,
                 exp_path: Optional[str] = None,
                 macros: Optional[Mapping[str, Collection[str]]] = None):
        self._test_expectations = test_expectations
        macros = (
            macros
            or self._test_expectations.port.configuration_specifier_macros())
        self._versions_by_os = {
            os.lower(): frozenset(version.lower() for version in os_versions)
            for os, os_versions in macros.items()
        }
        self._os_by_version = {
            version: os
            for os, versions in self._versions_by_os.items()
            for version in versions
        }
        port = self._test_expectations.port
        self._exp_path = (exp_path
                          or port.path_to_generic_test_expectations_file())
        self._tags_in_file = self._tags_in_expectation_file(
            self._exp_path,
            port.host.filesystem.read_text_file(self._exp_path))

    @property
    def _os_specifiers(self) -> FrozenSet[str]:
        return frozenset(self._versions_by_os)

    @property
    def _version_specifiers(self) -> FrozenSet[str]:
        return frozenset(self._os_by_version)

    def _tags_in_expectation_file(self, path, content):
        test_expectations = typ_types.TestExpectations()
        ret, errors = test_expectations.parse_tagged_list(
            content, path)
        if not ret:
            return set().union(*test_expectations.tag_sets)
        return set()

    def _resolve_versions(self, tags: FrozenSet[str]) -> FrozenSet[str]:
        tag = self._system_tag(tags)
        if tag == self.ALL_SYSTEMS:
            # A line without any OS/version specifiers applies to all versions.
            return self._version_specifiers
        return self._versions_by_os.get(tag, {tag})

    def _system_tag(self, tags: FrozenSet[str]) -> str:
        tags = frozenset(tag.lower() for tag in tags)
        maybe_version = tags & self._version_specifiers
        maybe_os = tags & self._os_specifiers
        if maybe_version:
            (version, ) = maybe_version
            return version
        elif maybe_os:
            (os, ) = maybe_os
            return os
        return self.ALL_SYSTEMS

    def _simplify_versions(self,
                           versions: FrozenSet[str]) -> Dict[str, Set[str]]:
        """Find a minimal set of system specifiers to write.

        Returns:
            A map from new system specifiers to old ones in `versions`. System
            specifiers may be at the OS or version level. Tags that could not
            be simplified are mapped identically (e.g., Mac -> {Mac}).
        """
        system_specifiers = defaultdict(set)
        for os, os_versions in self._versions_by_os.items():
            # If all the versions of an OS are in the system specifiers set, then
            # replace all those specifiers with the OS specifier.
            if os_versions <= versions:
                system_specifiers[os].update(os_versions)
        for version in versions - frozenset().union(
                *system_specifiers.values()):
            system_specifiers[version].add(version)
        if set(system_specifiers) >= self._os_specifiers:
            return {self.ALL_SYSTEMS: set(versions)}
        # Skip tags not listed in TestExpectations
        return {
            new_tag: old_tags
            for new_tag, old_tags in system_specifiers.items()
            if new_tag in self._tags_in_file and old_tags
        }

    def update_versions(self,
                        test_name: str,
                        versions: Collection[str],
                        results: Collection[ResultType],
                        reason: str = '',
                        marker: Optional[str] = None,
                        autotriage: bool = True) -> ExpectationsChange:
        """Update TestExpectations safely.

        Arguments:
            test_name: Test name to update.
            versions: Version specifiers that should receive the new results.
            results: Expected results.
            marker: The contents of a comment under which new expectations
                should be written if autotriaging is disabled or fails to find
                a related line. If the marker is not found or not provided,
                write new lines at the end of the file.
            autotriage: Attempt to write the new expectation near an existing
                related line, if possible.
        """
        versions = versions or self._version_specifiers
        versions = frozenset(version.lower() for version in versions)
        change = self.remove_os_versions(test_name, versions)
        expectations = self._test_expectations.get_expectations_from_file(
            self._exp_path, test_name)
        if autotriage:
            # Get expectations for this test with all specifiers matching except
            # for the system tag.
            expectations = [
                exp for exp in expectations
                if not ({tag.lower()
                         for tag in exp.tags} - {self._system_tag(exp.tags)})
                and exp.results == results
            ]
        else:
            expectations = []
        tags = sorted(self._system_tag(exp.tags) for exp in expectations)
        marker_line = self._find_marker(marker)
        for version in versions:
            anchor_exp = marker_line
            if expectations:
                index = bisect.bisect(tags, version)
                anchor_exp = expectations[max(0, index - 1)]
            new_exp = typ_types.Expectation(
                tags={version},
                results=results,
                is_slow_test=anchor_exp.is_slow_test,
                reason=(reason or anchor_exp.reason),
                test=test_name,
                lineno=anchor_exp.lineno,
                trailing_comments=anchor_exp.trailing_comments)
            change += self._test_expectations.add_expectations(
                self._exp_path, [new_exp], anchor_exp.lineno)
        return change

    def merge_versions(self, test_name: str) -> ExpectationsChange:
        """Merge test expectations for systems with the same results."""
        change = ExpectationsChange()
        expectations = self._test_expectations.get_expectations_from_file(
            self._exp_path, test_name)
        exps_by_other_tags = defaultdict(list)
        for exp in expectations:
            other_tags = frozenset(tag.lower() for tag in exp.tags)
            other_tags -= {self._system_tag(exp.tags)}
            exps_by_other_tags[other_tags, exp.results].append(exp)
        exps_to_remove = []
        # Try to collapse the group along the system tag dimension.
        for (other_tags, _), exp_group in exps_by_other_tags.items():
            exps_by_system_tags = {
                self._system_tag(exp.tags): exp
                for exp in exp_group
            }
            system_tags = self._simplify_versions(
                frozenset(exps_by_system_tags))
            for new_tag, old_tags in system_tags.items():
                exps_to_remove.extend(exps_by_system_tags[tag]
                                      for tag in old_tags if tag != new_tag)
                if new_tag not in old_tags:
                    new_tags = set(other_tags)
                    if new_tag != self.ALL_SYSTEMS:
                        new_tags.add(new_tag)
                    old_exps = [exps_by_system_tags[tag] for tag in old_tags]
                    new_exp = self._merge_expectations(old_exps, new_tags)
                    change += self._test_expectations.add_expectations(
                        self._exp_path, [new_exp], new_exp.lineno)
        change += self._test_expectations.remove_expectations(
            self._exp_path, exps_to_remove)
        return change

    def _merge_expectations(self, exps: List[typ_types.ExpectationType],
                            tags: FrozenSet[str]) -> typ_types.ExpectationType:
        reasons = {exp.reason.strip() for exp in exps}
        comments = set()
        for exp in exps:
            comment = exp.trailing_comments.strip()
            if comment.startswith('#'):
                comment = comment[1:].strip()
            if comment:
                comments.add(comment)
        new_comment = '  # ' + ', '.join(sorted(comments)) if comments else ''
        assert len({exp.test for exp in exps}) == 1
        assert len({exp.results for exp in exps}) == 1
        return typ_types.Expectation(
            lineno=exps[0].lineno,
            is_slow_test=any(exp.is_slow_test for exp in exps),
            reason=' '.join(sorted(reason for reason in reasons if reason)),
            trailing_comments=new_comment,
            test=exps[0].test,
            results=exps[0].results,
            tags=tags)

    def _find_marker(self,
                     marker: Optional[str] = None
                     ) -> typ_types.ExpectationType:
        lines = self._test_expectations.get_updated_lines(self._exp_path)
        if marker:
            for line in lines:
                contents = line.to_string().lstrip()
                if marker in contents and contents.startswith('#'):
                    return line
            exps = [_NotExpectation(f'# {marker}', 0)]
            self._test_expectations.add_expectations(self._exp_path, exps)
            lines = self._test_expectations.get_updated_lines(self._exp_path)
        return lines[-1]

    def remove_os_versions(
            self, test_name: str,
            versions_to_remove: Collection[str]) -> ExpectationsChange:
        """Remove system specifiers (e.g., `Mac10.10`) from expectations.

        This method will also split an expectation with no OS or OS version
        specifiers into expectations for OS versions that were not removed.
        These residual expectations are written with OS-family specifiers (e.g.,
        `Mac`) when possible.
        """
        change = ExpectationsChange()
        versions_to_remove = frozenset(
            specifier.lower() for specifier in versions_to_remove)
        if not versions_to_remove:
            # This will prevent making changes to test expectations which
            # have no OS versions to remove.
            return change

        expectations = self._test_expectations.get_expectations_from_file(
            self._exp_path, test_name)
        for exp in expectations:
            tags = frozenset(tag.lower() for tag in exp.tags)
            versions = self._resolve_versions(tags)
            if not versions & versions_to_remove:
                continue
            versions -= versions_to_remove
            other_specifiers = (tags - self._os_specifiers -
                                self._version_specifiers)
            systems = self._simplify_versions(versions)
            tag_sets = [({system} if system != self.ALL_SYSTEMS else set())
                        | other_specifiers for system in sorted(systems)]
            residual_exps = [
                typ_types.Expectation(tags=tags,
                                      results=exp.results,
                                      is_slow_test=exp.is_slow_test,
                                      reason=exp.reason,
                                      test=exp.test,
                                      lineno=exp.lineno,
                                      trailing_comments=exp.trailing_comments)
                for tags in tag_sets
            ]
            change += self._test_expectations.remove_expectations(
                self._exp_path, [exp])
            change += self._test_expectations.add_expectations(
                self._exp_path, residual_exps, exp.lineno)
        return change

    def update_expectations(self):
        self._test_expectations.commit_changes()


class TestExpectationsCache:
    def __init__(self):
        self._cache: Dict[Tuple[str, Optional[str]], TestExpectations] = {}

    def load(self, port: 'Port') -> TestExpectations:
        cache_key = port.name(), port.get_option('flag_specific')
        expectations = self._cache.get(cache_key)
        if not expectations:
            self._cache[cache_key] = expectations = TestExpectations(port)
        return expectations
