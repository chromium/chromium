// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/history/model/history_backend_client_impl.h"

#include "components/bookmarks/browser/history_bookmark_model.h"
#include "components/bookmarks/browser/model_loader.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "url/gurl.h"

HistoryBackendClientImpl::HistoryBackendClientImpl(
    scoped_refptr<bookmarks::ModelLoader> model_loader)
    : model_loader_(std::move(model_loader)) {}

HistoryBackendClientImpl::~HistoryBackendClientImpl() = default;

bool HistoryBackendClientImpl::IsPinnedURL(const GURL& url) {
  if (!model_loader_) {
    return false;
  }

  // HistoryBackendClient is used to determine if an URL is bookmarked. The
  // data is loaded on a separate thread and may not be done when this method
  // is called, therefore blocks until the bookmarks have finished loading.
  model_loader_->BlockTillLoaded();
  return model_loader_->history_bookmark_model()->IsBookmarked(url);
}

std::vector<history::URLAndTitle> HistoryBackendClientImpl::GetPinnedURLs() {
  if (!model_loader_) {
    return {};
  }

  // HistoryBackendClient is used to determine the set of bookmarked URLs. The
  // data is loaded on a separate thread and may not be done when this method
  // is called, therefore blocks until the bookmarks have finished loading.
  model_loader_->BlockTillLoaded();
  std::vector<bookmarks::UrlAndTitle> url_and_titles =
      model_loader_->history_bookmark_model()->GetUniqueUrls();

  std::vector<history::URLAndTitle> result;
  result.reserve(url_and_titles.size());
  for (auto& url_and_title : url_and_titles) {
    result.emplace_back(std::move(url_and_title.url),
                        std::move(url_and_title.title));
  }
  return result;
}

bool HistoryBackendClientImpl::IsWebSafe(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS();
}
