# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A set of Chromium Change-IDs that are known to have been successfully
exported to the upstream Web Platform Tests (WPT) repository, even if
Chromium's automated importer tooling might not correctly detect their
exported status from WPT's commit messages.

This set serves as a last-resort override primarily for the
Chromium WPT **importer** job and not the exporter job (more below).
When the importer identifies a Chromium Change-ID as needing to be
exported (e.g., because WPT's merged commit doesn't contain the
expected 'Cr-Commit-Position' or 'Change-Id' from Chromium's
perspective), but the change has actually been manually
verified as exported or needs to be ignored because it will not apply
cleanly, its Chromium Change-ID can be added to this set. This
allows the importer to proceed and not fail when trying to examine those
Change IDs.

Note about the WPT exporter job: The exported status is typically
managed by directly updating the GitHub Pull Request description
in WPT, which the exporter tooling already interprets correctly
even after the PR is merged.
"""

from typing import Optional, Set

# This set contains Chromium Change-IDs that should be considered
# as already exported to WPT by the importer job.
# Each ID should have a bug link.
_RAW_KNOWN_EXPORTED_CHANGE_IDS = {
    'I90107ade1e2e9f367afc283515b1899354ef2444',  # crbug.com/430622361
    'I4f1d099a85695eeb5e3cfd46c2f460540391bddd',  # crbug.com/430622361
    # https://crrev.com/c/6794665/comments/3f93724b_1ba128a7
    'Ie8bf2a14f5d22d692b45e85a1d3eb2bd0c34725f',
    # Forgot to put `No-Export: true` in the commit message for
    # https://crrev.com/c/6977528.
    'I7c368ddaabcc257192042770dfda59d18611c6cb',
}


class KnownExportedChangeIdsSet:
    """A set-like object for case-insensitive lookup of Chromium Change-IDs
    that are known to be exported to WPT.

    It encapsulates the storage and lookup logic, ensuring that
    Change-IDs are internally stored in lowercase for
    case-insensitive matching.
    """

    def __init__(self, raw_ids: Set[str]):
        """Initializes the set with a list of raw Change-IDs.
        All IDs are converted to lowercase for internal storage.
        """
        # Internally store all Change-IDs in lowercase for efficient lookups.
        self._ids = {change_id.lower() for change_id in raw_ids}

    def __contains__(self, change_id: Optional[str]) -> bool:
        """Checks if a Change-ID is in the set, performing a case-insensitive lookup.
        Non-string elements will always return False.
        """
        if not isinstance(change_id, str):
            return False
        return change_id.lower() in self._ids


# The singleton instance used throughout the system.
# It receives the raw set and processes it internally to build its lowercase set.
KNOWN_EXPORTED_CHANGE_IDS = KnownExportedChangeIdsSet(
    _RAW_KNOWN_EXPORTED_CHANGE_IDS)
