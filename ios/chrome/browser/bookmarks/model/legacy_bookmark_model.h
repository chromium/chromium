// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_H_

#include "base/memory/weak_ptr.h"
#include "components/bookmarks/browser/bookmark_model.h"

// TODO(crbug.com/326185948): Replace this with a minimalistic fork of
// BookmarkModel.
class LegacyBookmarkModel : public bookmarks::BookmarkModel {
 public:
  LegacyBookmarkModel(std::unique_ptr<bookmarks::BookmarkClient> client);
  ~LegacyBookmarkModel() override;

  base::WeakPtr<LegacyBookmarkModel> AsWeakPtr();

 private:
  base::WeakPtrFactory<LegacyBookmarkModel> legacy_weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_H_
