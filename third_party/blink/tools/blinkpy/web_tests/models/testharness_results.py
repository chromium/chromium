# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility module for checking testharness test output."""

_TESTHARNESSREPORT_HEADER = 'This is a testharness.js-based test.'
_TESTHARNESSREPORT_FOOTER = 'Harness: the test ran to completion.'


def is_all_pass_testharness_result(content_text):
    """Returns whether |content_text| is a testharness result that only contains PASS lines."""
    return (is_testharness_output(content_text)
            and is_testharness_output_passing(content_text)
            and not has_other_useful_output(content_text))


def is_testharness_output(content_text):
    """Returns whether |content_text| is a testharness output."""
    # Leading and trailing white spaces are accepted.
    lines = content_text.strip().splitlines()
    lines = [line.strip() for line in lines]

    # A testharness output is defined as containing the header and the footer.
    found_header = False
    found_footer = False
    for line in lines:
        if line == _TESTHARNESSREPORT_HEADER:
            found_header = True
        elif line == _TESTHARNESSREPORT_FOOTER:
            found_footer = True

    return found_header and found_footer


def is_testharness_output_passing(content_text):
    """Checks whether |content_text| is a passing testharness output.

    Under a relatively loose/accepting definition of passing
    testharness output, we consider any output with at least one
    PASS result and no FAIL result (or TIMEOUT or NOTRUN).
    """
    # Leading and trailing whitespace are ignored.
    lines = content_text.strip().splitlines()
    lines = [line.strip() for line in lines]

    at_least_one_pass = False

    for line in lines:
        if line.startswith('PASS'):
            at_least_one_pass = True
            continue
        if (line.startswith('FAIL') or line.startswith('TIMEOUT')
                or line.startswith('NOTRUN')
                or line.startswith('Harness Error.')):
            return False

    return at_least_one_pass


def has_other_useful_output(content_text):
    """Returns whether |content_text| has other useful output.

    Namely, console errors/warnings & alerts/confirms/prompts.
    """

    prefixes = ('CONSOLE ERROR:', 'CONSOLE WARNING:', 'ALERT:', 'CONFIRM:',
                'PROMPT:')

    def is_useful(line):
        return any(line.startswith(prefix) for prefix in prefixes)

    lines = content_text.strip().splitlines()
    return any(is_useful(line) for line in lines)
