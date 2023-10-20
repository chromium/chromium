# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility module for checking testharness test output."""

import enum
import functools
import re
from typing import FrozenSet, Iterator, List, NamedTuple, Optional, Tuple


class LineType(enum.Enum):
    HEADER = 'This is a testharness.js-based test.'
    FOOTER = 'Harness: the test ran to completion.'
    ALL_PASS = 'All subtests passed and are omitted for brevity.'
    SUBTEST = enum.auto()
    HARNESS_ERROR = 'Harness Error.'
    CONSOLE_ERROR = 'CONSOLE ERROR:'
    CONSOLE_WARNING = 'CONSOLE WARNING:'
    ALERT = 'ALERT:'
    CONFIRM = 'CONFIRM:'
    PROMPT = 'PROMPT:'


class Status(enum.Enum):
    PASS = enum.auto()
    FAIL = enum.auto()
    ERROR = enum.auto()
    TIMEOUT = enum.auto()
    NOTRUN = enum.auto()
    PRECONDITION_FAILED = enum.auto()


ABBREVIATED_ALL_PASS = '\n'.join([
    LineType.HEADER.value,
    LineType.ALL_PASS.value,
    'See https://chromium.googlesource.com/chromium/src/+/HEAD/'
    'docs/testing/writing_web_tests.md#Text-Test-Baselines for details.',
    LineType.FOOTER.value,
]) + '\n'


class TestharnessLine(NamedTuple):
    line_type: LineType
    statuses: FrozenSet[Status] = frozenset()
    # No message means it should not be checked.
    message: Optional[str] = None
    subtest: Optional[str] = None


_HARNESS_ERROR_PATTERN = re.compile(
    r'harness_status\.status = (?P<status>.*) , '
    r'harness_status\.message = (?P<message>.*)')
_HARNESS_ERROR_CODES = {
    1: Status.ERROR,
    2: Status.TIMEOUT,
    3: Status.PRECONDITION_FAILED,
}
_STATUS_UNION = '\s*(' + '|'.join(status.name for status in Status) + ')\s*'
_SUBTEST_PATTERN = re.compile(rf'^\[{_STATUS_UNION}(,{_STATUS_UNION})*\]')
_MESSAGE_PREFIX = ' ' * 2


@functools.lru_cache()
def parse_testharness_baseline(
        content_text: str) -> List[Tuple[TestharnessLine]]:
    # Leading and trailing white spaces are accepted.
    raw_lines = iter(content_text.strip().splitlines())
    next_line = next(raw_lines, None)
    lines = []
    while next_line is not None:
        line, next_line = next_line.lstrip(), next(raw_lines, None)
        subtest_match = _SUBTEST_PATTERN.match(line)
        if subtest_match:
            statuses = _parse_statuses(subtest_match)
            subtest = _unescape(line[subtest_match.end():])
            message = None
            if next_line and next_line.startswith(_MESSAGE_PREFIX):
                message = _unescape(next_line)
                next_line = next(raw_lines, None)
            lines.append(
                TestharnessLine(LineType.SUBTEST, statuses, message, subtest))
            continue
        maybe_type = {
            line_type
            for line_type in frozenset(LineType) - {LineType.SUBTEST}
            if line.startswith(line_type.value)
        }
        assert len(
            maybe_type) <= 1, f'line types {maybe_type} are not prefix-free'
        if maybe_type:
            line_type = maybe_type.pop()
            message, statuses = line[len(line_type.value):], frozenset()
            if line_type is LineType.HARNESS_ERROR:
                maybe_match = _HARNESS_ERROR_PATTERN.search(message)
                if maybe_match:
                    message = maybe_match['message']
                    status_code = int(maybe_match['status'])
                    statuses = frozenset([_HARNESS_ERROR_CODES[status_code]])
            message = _unescape(message) if message else None
            lines.append(TestharnessLine(line_type, statuses, message))
    return lines


def _parse_statuses(subtest_match: re.Match) -> FrozenSet[Status]:
    start, end = len('['), subtest_match.end() - len(']')
    return frozenset(Status[status.strip()]
                     for status in subtest_match.string[start:end].split(','))


_UNESCAPE_SUBSTITUTIONS = {
    r'\n': '\n',
    r'\r': '\r',
    r'\0': '\0',
}
# Add an extra backslash for `re`.
_UNESCAPE_PATTERN = re.compile('(' + '|'.join(
    literal.replace('\\', r'\\') for literal in _UNESCAPE_SUBSTITUTIONS) + ')')


def _unescape(s: str) -> str:
    return _UNESCAPE_PATTERN.sub(
        lambda match: _UNESCAPE_SUBSTITUTIONS[match[0]], s.lstrip())


def is_all_pass_testharness_result(content_text: str) -> bool:
    """Returns whether |content_text| is a testharness result that only contains PASS lines."""
    return (is_testharness_output(content_text)
            and is_testharness_output_passing(content_text)
            and not has_other_useful_output(content_text))


def is_testharness_output(content_text: str) -> bool:
    """Returns whether |content_text| is a testharness output."""
    # A testharness output is defined as containing the header and the footer.
    line_types = {
        line.line_type
        for line in parse_testharness_baseline(content_text)
    }
    return {LineType.HEADER, LineType.FOOTER} <= line_types


def is_testharness_output_passing(content_text: str) -> bool:
    """Checks whether |content_text| is a passing testharness output.

    Under a relatively loose/accepting definition of passing
    testharness output, we consider any output with at least one
    PASS result and no FAIL result (or TIMEOUT or NOTRUN).
    """
    at_least_one_pass = False
    for line in parse_testharness_baseline(content_text):
        if line.line_type is LineType.HARNESS_ERROR or line.statuses - {
                Status.PASS
        }:
            return False
        if line.line_type is LineType.ALL_PASS or Status.PASS in line.statuses:
            at_least_one_pass = True
    return at_least_one_pass


def has_other_useful_output(content_text: str) -> bool:
    """Returns whether |content_text| has other useful output.

    Namely, console errors/warnings & alerts/confirms/prompts.
    """
    line_types = {
        line.line_type
        for line in parse_testharness_baseline(content_text)
    }
    return bool(
        line_types & {
            LineType.CONSOLE_ERROR,
            LineType.CONSOLE_WARNING,
            LineType.ALERT,
            LineType.CONFIRM,
            LineType.PROMPT,
        })
