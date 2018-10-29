// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/manifest/manifest_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/strings/nullable_string16.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/fetchers/manifest_fetcher.h"
#include "content/renderer/manifest/manifest_parser.h"
#include "content/renderer/manifest/manifest_uma_util.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

// static
bool ManifestManager::CanFetchManifest(RenderFrame* render_frame) {
  // Do not fetch the manifest if we are on a unique origin.
  return !render_frame->GetWebFrame()
              ->GetDocument()
              .GetSecurityOrigin()
              .IsUnique();
}

ManifestManager::ManifestManager(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      may_have_manifest_(false),
      manifest_dirty_(true) {}

ManifestManager::~ManifestManager() {
  if (fetcher_)
    fetcher_->Cancel();

  // Consumers in the browser process will not receive this message but they
  // will be aware of the RenderFrame dying and should act on that. Consumers
  // in the renderer process should be correctly notified.
  ResolveCallbacks(ResolveStateFailure);
}

void ManifestManager::RequestManifest(RequestManifestCallback callback) {
  RequestManifestImpl(base::BindOnce(
      [](RequestManifestCallback callback, const GURL& manifest_url,
         const blink::Manifest& manifest,
         const blink::mojom::ManifestDebugInfo* debug_info) {
        std::move(callback).Run(manifest_url, manifest);
      },
      std::move(callback)));
}

void ManifestManager::RequestManifestDebugInfo(
    RequestManifestDebugInfoCallback callback) {
  RequestManifestImpl(base::BindOnce(
      [](RequestManifestDebugInfoCallback callback, const GURL& manifest_url,
         const blink::Manifest& manifest,
         const blink::mojom::ManifestDebugInfo* debug_info) {
        std::move(callback).Run(manifest_url,
                                debug_info ? debug_info->Clone() : nullptr);
      },
      std::move(callback)));
}

void ManifestManager::RequestManifestImpl(
    InternalRequestManifestCallback callback) {
  if (!may_have_manifest_) {
    std::move(callback).Run(GURL(), blink::Manifest(), nullptr);
    return;
  }

  if (!manifest_dirty_) {
    std::move(callback).Run(manifest_url_, manifest_,
                            manifest_debug_info_.get());
    return;
  }

  pending_callbacks_.push_back(std::move(callback));

  // Just wait for the running call to be done if there are other callbacks.
  if (pending_callbacks_.size() > 1)
    return;

  FetchManifest();
}

void ManifestManager::DidChangeManifest() {
  may_have_manifest_ = true;
  manifest_dirty_ = true;
  manifest_url_ = GURL();
  manifest_debug_info_ = nullptr;
}

void ManifestManager::DidCommitProvisionalLoad(bool is_same_document_navigation,
                                               ui::PageTransition transition) {
  if (is_same_document_navigation)
    return;

  may_have_manifest_ = false;
  manifest_dirty_ = true;
  manifest_url_ = GURL();
}

void ManifestManager::FetchManifest() {
  if (!CanFetchManifest(render_frame())) {
    ManifestUmaUtil::FetchFailed(ManifestUmaUtil::FETCH_FROM_UNIQUE_ORIGIN);
    ResolveCallbacks(ResolveStateFailure);
    return;
  }

  manifest_url_ = render_frame()->GetWebFrame()->GetDocument().ManifestURL();

  if (manifest_url_.is_empty()) {
    ManifestUmaUtil::FetchFailed(ManifestUmaUtil::FETCH_EMPTY_URL);
    ResolveCallbacks(ResolveStateFailure);
    return;
  }

  fetcher_.reset(new ManifestFetcher(manifest_url_));
  fetcher_->Start(
      render_frame()->GetWebFrame(),
      render_frame()->GetWebFrame()->GetDocument().ManifestUseCredentials(),
      base::Bind(&ManifestManager::OnManifestFetchComplete,
                 base::Unretained(this),
                 render_frame()->GetWebFrame()->GetDocument().Url()));
}

static const std::string& GetMessagePrefix() {
  static base::NoDestructor<std::string> message_prefix("Manifest: ");
  return *message_prefix;
}

void ManifestManager::OnManifestFetchComplete(
    const GURL& document_url,
    const blink::WebURLResponse& response,
    const std::string& data) {
  fetcher_.reset();
  if (response.IsNull() && data.empty()) {
    manifest_debug_info_ = nullptr;
    ManifestUmaUtil::FetchFailed(ManifestUmaUtil::FETCH_UNSPECIFIED_REASON);
    ResolveCallbacks(ResolveStateFailure);
    return;
  }

  ManifestUmaUtil::FetchSucceeded();
  GURL response_url = response.Url();
  base::StringPiece data_piece(data);
  ManifestParser parser(data_piece, response_url, document_url);
  parser.Parse();

  manifest_debug_info_ = blink::mojom::ManifestDebugInfo::New();
  manifest_debug_info_->raw_manifest = data;
  parser.TakeErrors(&manifest_debug_info_->errors);

  for (const auto& error : manifest_debug_info_->errors) {
    blink::WebConsoleMessage message;
    message.level = error->critical ? blink::WebConsoleMessage::kLevelError
                                    : blink::WebConsoleMessage::kLevelWarning;
    message.text =
        blink::WebString::FromUTF8(GetMessagePrefix() + error->message);
    message.url =
        render_frame()->GetWebFrame()->GetDocument().ManifestURL().GetString();
    message.line_number = error->line;
    message.column_number = error->column;
    render_frame()->GetWebFrame()->AddMessageToConsole(message);
  }

  // Having errors while parsing the manifest doesn't mean the manifest parsing
  // failed. Some properties might have been ignored but some others kept.
  if (parser.failed()) {
    ResolveCallbacks(ResolveStateFailure);
    return;
  }

  manifest_url_ = response.Url();
  manifest_ = parser.manifest();
  ResolveCallbacks(ResolveStateSuccess);
}

void ManifestManager::ResolveCallbacks(ResolveState state) {
  // Do not reset |manifest_url_| on failure here. If manifest_url_ is
  // non-empty, that means the link 404s, we failed to fetch it, or it was
  // unparseable. However, the site still tried to specify a manifest, so
  // preserve that information in the URL for the callbacks.
  // |manifest_url| will be reset on navigation or if we receive a didchange
  // event.
  if (state == ResolveStateFailure)
    manifest_ = blink::Manifest();

  manifest_dirty_ = state != ResolveStateSuccess;

  std::vector<InternalRequestManifestCallback> callbacks;
  swap(callbacks, pending_callbacks_);

  for (auto& callback : callbacks) {
    std::move(callback).Run(manifest_url_, manifest_,
                            manifest_debug_info_.get());
  }
}

void ManifestManager::BindToRequest(
    blink::mojom::ManifestManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void ManifestManager::OnDestruct() {}

}  // namespace content
