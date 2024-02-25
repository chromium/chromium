// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_UI_WEB_UI_URL_FETCHER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_UI_WEB_UI_URL_FETCHER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/url_fetcher.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace extensions {

// WebUIURLFetcher downloads the content of a file by giving its |url| on WebUI.
// Each WebUIURLFetcher is associated with a given |render_process_id,
// render_view_id| pair.
class WebUIURLFetcher : public URLFetcher {
 public:
  // Called when a file URL request is complete.
  // Parameters:
  // - whether the request is success.
  // - If yes, the content of the file.
  using WebUILoadFileCallback =
      base::OnceCallback<void(bool, std::unique_ptr<std::string>)>;

  WebUIURLFetcher(int render_process_id,
                  int render_frame_id,
                  const GURL& url,
                  WebUILoadFileCallback callback);

  WebUIURLFetcher(const WebUIURLFetcher&) = delete;
  WebUIURLFetcher& operator=(const WebUIURLFetcher&) = delete;

  ~WebUIURLFetcher() override;

  void Start() override;

 private:
  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  int render_process_id_;
  int render_frame_id_;
  GURL url_;
  WebUILoadFileCallback callback_;
  std::unique_ptr<network::SimpleURLLoader> fetcher_;

  base::WeakPtrFactory<WebUIURLFetcher> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_UI_WEB_UI_URL_FETCHER_H_
