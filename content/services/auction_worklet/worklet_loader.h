// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_LOADER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_LOADER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace auction_worklet {

class AuctionV8Helper;
class AuctionDownloader;

// Utility class to download and build a worklet.
class WorkletLoader {
 public:
  // On success, `worklet_script` is compiled script, not bound to any context.
  // It can be repeatedly bound to different contexts and executed, without
  // persisting any state.
  using LoadWorkletCallback = base::OnceCallback<void(
      std::unique_ptr<v8::Global<v8::UnboundScript>> worklet_script,
      absl::optional<std::string> error_msg)>;

  // Starts loading the worklet script on construction. Callback will be invoked
  // asynchronously once the data has been fetched or an error has occurred.
  // Must be destroyed before `v8_helper`.
  WorkletLoader(network::mojom::URLLoaderFactory* url_loader_factory,
                const GURL& script_source_url,
                AuctionV8Helper* v8_helper,
                LoadWorkletCallback load_worklet_callback);
  explicit WorkletLoader(const WorkletLoader&) = delete;
  WorkletLoader& operator=(const WorkletLoader&) = delete;
  ~WorkletLoader();

 private:
  void OnDownloadComplete(std::unique_ptr<std::string> body,
                          absl::optional<std::string> error_msg);

  const GURL script_source_url_;
  AuctionV8Helper* const v8_helper_;

  std::unique_ptr<AuctionDownloader> auction_downloader_;
  LoadWorkletCallback load_worklet_callback_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_LOADER_H_
