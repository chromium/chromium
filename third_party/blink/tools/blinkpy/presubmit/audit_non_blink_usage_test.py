#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import unittest

from blinkpy.presubmit import audit_non_blink_usage


class TestAuditNonBlinkUsageTest(unittest.TestCase):
    # This is not great but it allows us to check that something is a regexp.
    _REGEXP_CLASS = re.compile(r"foo").__class__

    def test_valid_compiled_config(self):
        # We need to test this protected data.
        # pylint: disable=W0212
        for entry in audit_non_blink_usage._COMPILED_CONFIG:
            for path in entry['paths']:
                self.assertIsInstance(path, str)
            if 'allowed' in entry:
                self.assertIsInstance(entry['allowed'], self._REGEXP_CLASS)
            if 'disallowed' in entry:
                self.assertIsInstance(entry['disallowed'], self._REGEXP_CLASS)
            for match, advice in entry.get('advice', []):
                self.assertIsInstance(match, self._REGEXP_CLASS)
                self.assertIsInstance(advice, str)


if __name__ == '__main__':
    unittest.main()
