# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class MockKnownExportedChangeIds:
    """A mock class for KNOWN_EXPORTED_CHANGE_IDS with a canned list of IDs."""

    def __init__(self, known_ids={}):
        """Initializes the mock with a list of Change-IDs to 'find'."""
        self._ids = known_ids

    def __contains__(self, change_id):
        """Checks if the given change_id was provided during initialization."""
        return change_id in self._ids
