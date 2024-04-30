// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_CONTROLLED_FRAME_EMBEDDER_URL_FETCHER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_CONTROLLED_FRAME_EMBEDDER_URL_FETCHER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/url_fetcher.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace extensions {

// ControlledFrameEmbedderURLFetcher downloads the content of a file by giving
// its |url| in the app.  Each ControlledFrameEmbedderURLFetcher is associated
// with a given |render_process_id, render_view_id| pair.
class ControlledFrameEmbedderURLFetcher : public extensions::URLFetcher {
 public:
  // Called when a file URL request is complete.
  // Parameters:
  // - whether the request is success.
  // - If yes, the content of the file.
  using ControlledFrameEmbedderLoadFileCallback =
      base::OnceCallback<void(bool, std::unique_ptr<std::string>)>;

  ControlledFrameEmbedderURLFetcher(
      int render_process_id,
      int render_frame_id,
      const GURL& url,
      ControlledFrameEmbedderLoadFileCallback callback);

  ControlledFrameEmbedderURLFetcher(const ControlledFrameEmbedderURLFetcher&) =
      delete;
  ControlledFrameEmbedderURLFetcher& operator=(
      const ControlledFrameEmbedderURLFetcher&) = delete;

  ~ControlledFrameEmbedderURLFetcher() override;

  void Start() override;

 private:
  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  int render_process_id_;
  int render_frame_id_;
  GURL url_;
  ControlledFrameEmbedderLoadFileCallback callback_;
  std::unique_ptr<network::SimpleURLLoader> fetcher_;

  base::WeakPtrFactory<ControlledFrameEmbedderURLFetcher> weak_ptr_factory_{
      this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_CONTROLLED_FRAME_EMBEDDER_URL_FETCHER_H_
