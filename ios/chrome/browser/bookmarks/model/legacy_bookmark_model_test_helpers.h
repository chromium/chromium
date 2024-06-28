// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_TEST_HELPERS_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_TEST_HELPERS_H_

class LegacyBookmarkModel;

// Blocks until `model` finishes loading.
void WaitForLegacyBookmarkModelToLoad(LegacyBookmarkModel* model);

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_TEST_HELPERS_H_
