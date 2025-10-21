# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test data corpus for automatic_ownership integration tests."""

# This fake git log is designed to trigger the Z-Score analysis for the
# 'ios/chrome/browser/ui' directory. It contains 12 commits, ensuring the
# commit count ( > 5) is high enough to avoid the git blame fallback.
#
# The contributions are heavily skewed toward user_a and user_b to ensure
# they are statistical outliers.
#
# Note: Commit `ccccccc1` contains the phrase "Fix typo", which is an ignored
# keyword in `filters.py`. This commit and its review are intentionally
# filtered out by the script.
#
# Expected stats for 'ios/chrome/browser/ui' (after filtering):
# - Total Commits: 11
# - Total Reviews: 11
# - user_a: 8 commits, 2 reviews
# - user_b: 2 commits, 7 reviews
# - user_c: 0 commits, 1 review
# - user_d: 1 commit, 1 review
#
# With these stats, the Z-score calculation should identify 'user_a' and
# 'user_b' as the owners.
FAKE_GIT_LOG = """
commit aaaaaaa1
Author: User A <user_a@chromium.org>
Date:   Mon Jan 1 10:00:00 2025 -0700

    Create feature files.

    Initial setup of files for feature.

  Change-Id: aaaaaaa1
  Reviewed-by: User B <user_b@chromium.org>
 ios/chrome/browser/feature/file1.mm | 10 +++++-----

commit aaaaaaa2
Author: User A <user_a@chromium.org>
Date:   Mon Jan 1 10:00:00 2025 -0700

    Add frobnitz support.

    Adds frobnitz support by means of whatsit observation.

    Change-Id: aaaaaaa2
    Reviewed-by: User B <user_b@chromium.org>
 ios/chrome/browser/feature/file1.mm | 10 +++++-----

commit aaaaaaa3
Author: User A <user_a@chromium.org>
Date:   Mon Jan 1 10:00:00 2025 -0700

    Refactor feature logic.

    Moves the core logic into a helper class.

    Change-Id: aaaaaaa3
    Reviewed-by: User B <user_b@chromium.org>
 ios/chrome/browser/feature/file1.mm | 10 +++++-----

commit aaaaaaa4
Author: User A <user_a@chromium.org>
Date:   Mon Jan 1 10:00:00 2025 -0700

    Add unit tests for feature.

    Improves test coverage for the main feature class.

    Change-Id: aaaaaaa4
    Reviewed-by: User B <user_b@chromium.org>
 ios/chrome/browser/feature/file1.mm | 10 +++++-----

commit aaaaaaa5
Author: User A <user_a@chromium.org>
Date:   Mon Jan 1 10:00:00 2025 -0700

    Fix off-by-one error in frobnitz.

    The frobnitz was not correctly handling the last element.

    Change-Id: aaaaaaa5
    Reviewed-by: User C <user_c@chromium.org>
 ios/chrome/browser/feature/file1.mm | 10 +++++-----

commit aaaaaaa6
Author: User A <user_a@chromium.org>
Date:   Mon Jan 1 10:00:00 2025 -0700

    Update UI constants.

    Changes the color and font size to match the new spec.

    Change-Id: aaaaaaa6
    Reviewed-by: User D <user_d@chromium.org>
 ios/chrome/browser/feature/file1.mm | 10 +++++-----

commit aaaaaaa7
Author: User A <user_a@chromium.org>
Date:   Mon Jan 1 10:00:00 2025 -0700

    Add accessibility labels.

    Ensures all UI elements have correct accessibility labels.

    Change-Id: aaaaaaa7
    Reviewed-by: User B <user_b@chromium.org>
 ios/chrome/browser/feature/file1.mm | 10 +++++-----

commit aaaaaaa8
Author: User A <user_a@chromium.org>
Date:   Mon Jan 1 10:00:00 2025 -0700

    Handle landscape orientation.

    The UI now correctly adapts to landscape mode.

    Change-Id: aaaaaaa8
    Reviewed-by: User B <user_b@chromium.org>
 ios/chrome/browser/feature/file1.mm | 10 +++++-----

commit bbbbbbb1
Author: User B <user_b@chromium.org>
Date:   Fri Jan 5 14:00:00 2025 -0700

    Introduce new data model.

    Adds a new data model for the feature backend.

    Change-Id: bbbbbbb1
    Reviewed-by: User A <user_a@chromium.org>
 ios/chrome/browser/feature/file5.mm | 5 +++++

commit bbbbbbb2
Author: User B <user_b@chromium.org>
Date:   Sat Jan 6 15:00:00 2025 -0700

    Implement data model caching.

    Caches the data model to improve performance.

    Change-Id: bbbbbbb2
    Reviewed-by: User A <user_a@chromium.org>
 ios/chrome/browser/feature/file6.mm | 5 +++++

commit ccccccc1
Author: User C <user_c@chromium.org>
Date:   Sun Jan 7 16:00:00 2025 -0700

    Fix typo in user-facing string.

    Corrects a spelling mistake in the main title.
    This is a trivial change that will not be counted for
    OWNER stats.

    Change-Id: ccccccc1
    Reviewed-by: User B <user_b@chromium.org>
 ios/chrome/browser/feature/file7.mm | 2 +-

commit ddddddd1
Author: User D <user_d@chromium.org>
Date:   Sun Jan 7 16:00:00 2025 -0700

    Remove unused import.

    Cleans up the file by removing an unnecessary import.

    Change-Id: ddddddd1
    Reviewed-by: User B <user_b@chromium.org>
 ios/chrome/browser/feature/file7.mm | 2 +-
"""
