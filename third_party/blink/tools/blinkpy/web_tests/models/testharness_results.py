# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility module for checking testharness test output."""

import enum
import functools
import re
from collections import Counter
from typing import FrozenSet, Iterator, List, NamedTuple, Optional, Tuple


class LineType(enum.Enum):
    TESTHARNESS_HEADER = 'This is a testharness.js-based test.'
    WDSPEC_HEADER = 'This is a wdspec test.'
    FOOTER = 'Harness: the test ran to completion.'
    ALL_PASS = 'All subtests passed and are omitted for brevity.'
    SUBTEST = enum.auto()
    HARNESS_ERROR = 'Harness Error.'
    CONSOLE_ERROR = 'CONSOLE ERROR:'
    CONSOLE_WARNING = 'CONSOLE WARNING:'
    ALERT = 'ALERT:'
    CONFIRM = 'CONFIRM:'
    PROMPT = 'PROMPT:'


class Status(enum.IntEnum):
    # Harness error code numbers. Some can also be subtest statuses, but they're
    # numbered differently.
    ERROR = 1
    TIMEOUT = 2
    PRECONDITION_FAILED = 3
    PASS = enum.auto()
    FAIL = enum.auto()
    NOTRUN = enum.auto()


ABBREVIATED_ALL_PASS = '\n'.join([
    LineType.TESTHARNESS_HEADER.value,
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


_HARNESS_ERROR_FORMAT = ('harness_status.status = %s , '
                         'harness_status.message = %s')
_HARNESS_ERROR_PATTERN = re.compile(
    re.escape(_HARNESS_ERROR_FORMAT) % ('(?P<status>.*)', '(?P<message>.*)'))
_STATUS_UNION = '\s*(' + '|'.join(status.name for status in Status) + ')\s*'
_SUBTEST_PATTERN = re.compile(rf'^\[{_STATUS_UNION}(,{_STATUS_UNION})*\]')
_MESSAGE_PREFIX = ' ' * 2
# Threshold at which a "Found [N] tests; ..." line will be written.
_COUNT_THRESHOLD = 50


@functools.lru_cache()
def parse_testharness_baseline(content_text: str) -> List[TestharnessLine]:
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
                    statuses = frozenset([Status(status_code)])
            message = _unescape(message) if message else None
            lines.append(TestharnessLine(line_type, statuses, message))
    return lines


def compact_test_output(lines: List[TestharnessLine]) -> List[TestharnessLine]:
    """Returns a compact output for reflection test results.

    The reflection tests contain a large number of tests.
    This test output merges PASS lines to make baselines smaller.
    """
    def maybe_append_pass_line(compact_lines, prefix, subtest, passes):
        if passes > 1:
            compact_lines.append(
                TestharnessLine(LineType.SUBTEST, {Status.PASS}, None,
                                f'{prefix}: {passes} tests'))
        elif passes == 1:
            compact_lines.append(
                TestharnessLine(LineType.SUBTEST, {Status.PASS}, None,
                                subtest))

    compact_lines = []
    prev_prefix = None
    prev_subtest = None
    prev_passes = 0
    for line in lines:
        if (line.line_type != LineType.SUBTEST
                or line.statuses != set([Status.PASS])
                or ':' not in line.subtest):
            maybe_append_pass_line(compact_lines, prev_prefix, prev_subtest,
                                   prev_passes)
            prev_passes = 0
            compact_lines.append(line)
            continue

        # We get a PASS subtest with ':' in test name
        prefix, suffix = line.subtest.split(':', 1)
        if prefix != prev_prefix:
            maybe_append_pass_line(compact_lines, prev_prefix, prev_subtest,
                                   prev_passes)
            prev_prefix, prev_subtest = prefix, line.subtest
            prev_passes = 1
        else:
            prev_passes += 1

    maybe_append_pass_line(compact_lines, prev_prefix, prev_subtest,
                           prev_passes)
    return compact_lines


def format_testharness_baseline(lines: List[TestharnessLine],
                                do_compact: bool) -> str:
    """Format testharness.js results in the same way as [0].

    [0]: //third_party/blink/web_tests/resources/testharnessreport.js
    """
    content = ''
    status_order = [Status.PASS, Status.FAIL, Status.TIMEOUT, Status.NOTRUN]
    status_counts = Counter({status: 0 for status in status_order})
    for line in lines:
        try:
            # For the status counts below the header, only count an arbitrary
            # (but well-defined) status when there are multiple so that the
            # total is correct. This is OK because the counts are not parsed and
            # are informational only.
            status = min(line.statuses, key=status_order.index)
            status_counts[status] += 1
        except ValueError:
            pass

    if do_compact:
        lines = compact_test_output(lines)

    for line in lines:
        if line.line_type is LineType.SUBTEST:
            assert line.subtest and line.statuses, line
            statuses = ', '.join(
                sorted(status.name for status in line.statuses))
            content += f'[{statuses}] {_escape(line.subtest)}\n'
            if line.message:
                content += f'{_MESSAGE_PREFIX}{_escape(line.message)}\n'
        elif line.line_type is LineType.HARNESS_ERROR:
            (status, ) = line.statuses
            assert isinstance(status.value, int), line
            harness_error = _HARNESS_ERROR_FORMAT % (
                str(status.value),
                _escape(line.message or ''),
            )
            content += f'{line.line_type.value} {harness_error}\n'
        else:
            content += line.line_type.value
            if line.message:
                content += f' {_escape(line.message)}'
            content += '\n'
        total = sum(status_counts.values())
        if (line.line_type is LineType.TESTHARNESS_HEADER
                and status_counts[Status.PASS] < total
                and total >= _COUNT_THRESHOLD):
            content += f'Found {total} tests; '
            content += ', '.join(
                f'{count} {status.name}'
                for status, count in status_counts.items()) + '.\n'
    return content


def _parse_statuses(subtest_match: re.Match) -> FrozenSet[Status]:
    start, end = len('['), subtest_match.end() - len(']')
    return frozenset(Status[status.strip()]
                     for status in subtest_match.string[start:end].split(','))


_UNESCAPE_SUBSTITUTIONS = {
    r'\n': '\n',
    r'\r': '\r',
    r'\0': '\0',
}
_ESCAPE_SUBSTITUTIONS = str.maketrans({
    unescaped: escaped
    for escaped, unescaped in _UNESCAPE_SUBSTITUTIONS.items()
})
# Add an extra backslash for `re`.
_UNESCAPE_PATTERN = re.compile('(' + '|'.join(
    literal.replace('\\', r'\\') for literal in _UNESCAPE_SUBSTITUTIONS) + ')')


def _escape(s: str) -> str:
    return s.translate(_ESCAPE_SUBSTITUTIONS)


def _unescape(s: str) -> str:
    return _UNESCAPE_PATTERN.sub(
        lambda match: _UNESCAPE_SUBSTITUTIONS[match[0]], s.lstrip())


def is_all_pass_test_result(content_text: str) -> bool:
    """Returns whether |content_text| is a testharness result that only contains PASS lines."""
    return ((is_testharness_output(content_text)
             or is_wdspec_output(content_text))
            and is_test_output_passing(content_text)
            and not has_other_useful_output(content_text))


def is_testharness_output(content_text: str) -> bool:
    """Returns whether |content_text| is a testharness output."""
    # A testharness output is defined as containing the header and the footer.
    line_types = {
        line.line_type
        for line in parse_testharness_baseline(content_text)
    }
    return {LineType.TESTHARNESS_HEADER, LineType.FOOTER} <= line_types


def is_wdspec_output(content_text: str) -> bool:
    """Returns whether |content_text| is a wdspec test output."""
    # A wdspec test output is defined as containing WDSPEC_HEADER and the footer.
    line_types = {
        line.line_type
        for line in parse_testharness_baseline(content_text)
    }
    return {LineType.WDSPEC_HEADER, LineType.FOOTER} <= line_types


def is_test_output_passing(content_text: str) -> bool:
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
