// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"

#include <utility>

LegacyBookmarkModel::LegacyBookmarkModel(
    std::unique_ptr<bookmarks::BookmarkClient> client)
    : BookmarkModel(std::move(client)) {}

LegacyBookmarkModel::~LegacyBookmarkModel() = default;

base::WeakPtr<LegacyBookmarkModel> LegacyBookmarkModel::AsWeakPtr() {
  return legacy_weak_factory_.GetWeakPtr();
}
