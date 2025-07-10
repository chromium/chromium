# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.w3c.known_exported_change_ids import KnownExportedChangeIdsSet


class KnownExportedChangeIdsSetTest(unittest.TestCase):
    """Unit tests for the KnownExportedChangeIdsSet class."""

    def test_init_and_contains(self):
        """Tests that the set is initialized correctly and performs case-insensitive containment checks."""
        # Instantiate with mixed-case initial IDs
        checker = KnownExportedChangeIdsSet({
            'IdtestId1',
            'IDTESTID2',
            'IDtEsTId3',
        })

        # Test containment with various casings: all should return True
        self.assertIn('IdtestId1', checker)
        self.assertIn('idtestid1', checker)
        self.assertIn('IDTESTID1', checker)

        self.assertIn('IdtestId2', checker)
        self.assertIn('idtestid2', checker)
        self.assertIn('IDTESTID2', checker)

        self.assertIn('IdtestId3', checker)
        self.assertIn('idtestid3', checker)
        self.assertIn('IDTESTID3', checker)

        # Test non-existent IDs: all should return False
        self.assertNotIn('NotThere', checker)
        self.assertNotIn('IdtestId4', checker)
        self.assertNotIn('IDTESTID4', checker)

        # Verify internal storage: all IDs should be stored as lowercase
        self.assertEqual(checker._ids, {'idtestid1', 'idtestid2', 'idtestid3'})
