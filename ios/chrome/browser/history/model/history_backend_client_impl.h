// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_MODEL_HISTORY_BACKEND_CLIENT_IMPL_H_
#define IOS_CHROME_BROWSER_HISTORY_MODEL_HISTORY_BACKEND_CLIENT_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "components/history/core/browser/history_backend_client.h"

class GURL;

namespace bookmarks {
class ModelLoader;
}

class HistoryBackendClientImpl : public history::HistoryBackendClient {
 public:
  // `model_loader` may be nullptr.
  explicit HistoryBackendClientImpl(
      scoped_refptr<bookmarks::ModelLoader> model_loader);
  HistoryBackendClientImpl(const HistoryBackendClientImpl&) = delete;
  HistoryBackendClientImpl& operator=(const HistoryBackendClientImpl&) = delete;

  ~HistoryBackendClientImpl() override;

 private:
  // history::HistoryBackendClient implementation.
  bool IsPinnedURL(const GURL& url) override;
  std::vector<history::URLAndTitle> GetPinnedURLs() override;
  bool IsWebSafe(const GURL& url) override;

  // ModelLoader is used to access bookmarks.
  const scoped_refptr<bookmarks::ModelLoader> model_loader_;
};

#endif  // IOS_CHROME_BROWSER_HISTORY_MODEL_HISTORY_BACKEND_CLIENT_IMPL_H_
