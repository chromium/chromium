// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/history/history_backend_client_impl.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "components/bookmarks/browser/history_bookmark_model.h"
#include "components/bookmarks/browser/model_loader.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "url/gurl.h"

HistoryBackendClientImpl::HistoryBackendClientImpl(
    std::vector<scoped_refptr<bookmarks::ModelLoader>> model_loaders)
    : model_loaders_(std::move(model_loaders)) {
  CHECK(!base::Contains(model_loaders_, nullptr));
}

HistoryBackendClientImpl::~HistoryBackendClientImpl() = default;

bool HistoryBackendClientImpl::IsPinnedURL(const GURL& url) {
  for (auto& model_loader : model_loaders_) {
    // HistoryBackendClient is used to determine if an URL is bookmarked. The
    // data is loaded on a separate thread and may not be done when this method
    // is called, therefore blocks until the bookmarks have finished loading.
    model_loader->BlockTillLoaded();
    if (model_loader->history_bookmark_model()->IsBookmarked(url)) {
      return true;
    }
  }
  return false;
}

std::vector<history::URLAndTitle> HistoryBackendClientImpl::GetPinnedURLs() {
  std::vector<history::URLAndTitle> result;
  for (auto& model_loader : model_loaders_) {
    // HistoryBackendClient is used to determine the set of bookmarked URLs. The
    // data is loaded on a separate thread and may not be done when this method
    // is called, therefore blocks until the bookmarks have finished loading.
    model_loader->BlockTillLoaded();
    std::vector<bookmarks::UrlAndTitle> url_and_titles =
        model_loader->history_bookmark_model()->GetUniqueUrls();
    result.reserve(result.size() + url_and_titles.size());
    for (auto& url_and_title : url_and_titles) {
      result.push_back(history::URLAndTitle{std::move(url_and_title.url),
                                            std::move(url_and_title.title)});
    }
  }
  return result;
}

bool HistoryBackendClientImpl::IsWebSafe(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS();
}
