// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/worklet_loader.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "content/services/auction_worklet/auction_downloader.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace auction_worklet {

WorkletLoader::WorkletLoader(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& script_source_url,
    AuctionV8Helper* v8_helper,
    LoadWorkletCallback load_worklet_callback)
    : script_source_url_(script_source_url),
      v8_helper_(v8_helper),
      load_worklet_callback_(std::move(load_worklet_callback)) {
  DCHECK(load_worklet_callback_);

  auction_downloader_ = std::make_unique<AuctionDownloader>(
      url_loader_factory, script_source_url,
      AuctionDownloader::MimeType::kJavascript,
      base::BindOnce(&WorkletLoader::OnDownloadComplete,
                     base::Unretained(this)));
}

WorkletLoader::~WorkletLoader() = default;

void WorkletLoader::OnDownloadComplete(std::unique_ptr<std::string> body) {
  DCHECK(load_worklet_callback_);

  auction_downloader_.reset();

  if (!body) {
    std::move(load_worklet_callback_).Run(nullptr /* worklet_script */);
    return;
  }

  std::unique_ptr<v8::Global<v8::UnboundScript>> global_script;

  // Need to release the isolate and context before invoking the callback, in
  // case the `v8_helper_` is destroyed.
  {
    AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_);
    v8::Context::Scope context_scope(v8_helper_->scratch_context());

    v8::Local<v8::UnboundScript> local_script;
    if (v8_helper_->Compile(*body, script_source_url_).ToLocal(&local_script)) {
      global_script = std::make_unique<v8::Global<v8::UnboundScript>>(
          v8_helper_->isolate(), local_script);
    }
  }

  std::move(load_worklet_callback_).Run(std::move(global_script));
}

}  // namespace auction_worklet
