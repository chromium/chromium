// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_SHARED_STORAGE_WORKLET_MODULE_SCRIPT_DOWNLOADER_H_
#define CONTENT_SERVICES_SHARED_STORAGE_WORKLET_MODULE_SCRIPT_DOWNLOADER_H_

#include <string>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}

namespace shared_storage_worklet {

// Download utility for worklet module script. Creates requests and blocks
// responses.
class CONTENT_EXPORT ModuleScriptDownloader {
 public:
  // Passes in nullptr on failure. Always invoked asynchronously.
  using ModuleScriptDownloaderCallback =
      base::OnceCallback<void(std::unique_ptr<std::string> response_body,
                              std::string error_message)>;

  // Starts loading the worklet module script on construction. Callback will be
  // invoked asynchronously once the data has been fetched or an error has
  // occurred.
  ModuleScriptDownloader(
      network::mojom::URLLoaderFactory* url_loader_factory,
      const GURL& source_url,
      ModuleScriptDownloaderCallback module_script_downloader_callback);

  ModuleScriptDownloader(const ModuleScriptDownloader&) = delete;
  ModuleScriptDownloader& operator=(const ModuleScriptDownloader&) = delete;

  ~ModuleScriptDownloader();

 private:
  void OnBodyReceived(std::unique_ptr<std::string> body);

  void OnRedirect(const net::RedirectInfo& redirect_info,
                  const network::mojom::URLResponseHead& response_head,
                  std::vector<std::string>* removed_headers);

  const GURL source_url_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  ModuleScriptDownloaderCallback module_script_downloader_callback_;
};

}  // namespace shared_storage_worklet

#endif  // CONTENT_SERVICES_SHARED_STORAGE_WORKLET_MODULE_SCRIPT_DOWNLOADER_H_
