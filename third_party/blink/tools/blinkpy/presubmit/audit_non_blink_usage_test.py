#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import unittest

from blinkpy.presubmit import audit_non_blink_usage as audit


class TestAuditNonBlinkUsageTest(unittest.TestCase):
    # This is not great but it allows us to check that something is a regexp.
    _REGEXP_CLASS = re.compile(r"foo").__class__

    def test_valid_compiled_config(self):
        # We need to test this protected data.
        # pylint: disable=W0212
        for entry in audit._COMPILED_CONFIG:
            for path in entry['paths']:
                self.assertIsInstance(path, str)
            if 'allowed' in entry:
                self.assertIsInstance(entry['allowed'], self._REGEXP_CLASS)
            if 'inclass_allowed' in entry:
                self.assertIsInstance(entry['inclass_allowed'],
                                      self._REGEXP_CLASS)
            if 'inclass_disallowed' in entry:
                self.assertIsInstance(entry['inclass_disallowed'],
                                      self._REGEXP_CLASS)
            for match, advice, warning in entry.get('advice', []):
                self.assertIsInstance(match, self._REGEXP_CLASS)
                self.assertIsInstance(advice, str)
            for match, advice, warning in entry.get('inclass_advice', []):
                self.assertIsInstance(match, self._REGEXP_CLASS)
                self.assertIsInstance(advice, str)

    def test_for_special_cases(self):
        check_list = [
            {
                'type': 'url::mojom::Origin',
                'allowed': False,
                'path': 'third_party/blink/renderer/'
            },
            {
                'type': 'media::mojom::InterfaceFactory',
                'allowed': False,
                'path': 'third_party/blink/renderer/'
            },
            {
                'type': 'url::mojom::blink::Origin',
                'allowed': True,
                'path': 'third_party/blink/renderer/'
            },
            {
                'type': 'media::mojom::blink::InterfaceFactory',
                'allowed': True,
                'path': 'third_party/blink/renderer/'
            },
            {
                'type': 'network::mojom::URLLoaderFactory',
                'allowed': True,
                'path': 'third_party/blink/renderer/'
            },
            {
                'type': 'base::SingleThreadTaskRunner::GetCurrentDefault',
                'allowed': False,
                'path': 'third_party/blink/renderer/'
            },
            {
                'type': 'base::SingleThreadTaskRunner::CurrentDefaultHandle',
                'allowed': False,
                'path': 'third_party/blink/renderer/'
            },
            {
                'type': 'base::SingleThreadTaskRunner',
                'allowed': True,
                'path': 'third_party/blink/renderer/'
            },
            {
                'type': 'base::SingleThreadTaskRunner::OtherAPI',
                'allowed': True,
                'path': 'third_party/blink/renderer/'
            },
            {
                'type': 'base::SingleThreadTaskRunner::GetCurrentDefault',
                'allowed': True,
                'path': 'third_party/blink/renderer/modules/mediarecorder/'
            },
            {
                'type': 'base::SingleThreadTaskRunner::CurrentDefaultHandle',
                'allowed': True,
                'path': 'third_party/blink/renderer/modules/mediarecorder/'
            },
            {
                'type': 'isolate->GetContinuationPreservedEmbedderData',
                'allowed': False,
                'path': 'third_party/blink/renderer/'
            },
            {
                'type': 'isolate->GetContinuationPreservedEmbedderData',
                'allowed': True,
                'path': 'third_party/blink/renderer/core/scheduler/'
            },
        ]
        for item in check_list:
            # Make sure that the identifier we're testing is parsed
            # fully.
            self.assertTrue(
                audit._IDENTIFIER_IN_CLASS_RE.fullmatch(item['type']) is
                not None or audit._IDENTIFIER_WITH_NAMESPACE_RE.fullmatch(
                    item['type']) is not None)
            # Use the mechanism in the presubmit source code to find
            # which rules to match.
            entries = audit._find_matching_entries(item['path'])
            in_class = audit._IDENTIFIER_WITH_NAMESPACE_RE.fullmatch(
                item['type']) is None
            allowed = audit._check_entries_for_identifier(
                entries, item['type'], in_class)
            self.assertEqual(allowed, item['allowed'])


if __name__ == '__main__':
    unittest.main()
