// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_manager.h"

#include <utility>

#include "base/bind.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/manifest/manifest_change_notifier.h"
#include "third_party/blink/renderer/modules/manifest/manifest_fetcher.h"
#include "third_party/blink/renderer/modules/manifest/manifest_parser.h"
#include "third_party/blink/renderer/modules/manifest/manifest_type_converters.h"
#include "third_party/blink/renderer/modules/manifest/manifest_uma_util.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

namespace blink {

// static
const char ManifestManager::kSupplementName[] = "ManifestManager";

// static
void WebManifestManager::RequestManifestForTesting(WebLocalFrame* web_frame,
                                                   Callback callback) {
  LocalFrame* frame = To<WebLocalFrameImpl>(web_frame)->GetFrame();
  ManifestManager* manifest_manager = ManifestManager::From(*frame);
  manifest_manager->RequestManifestForTesting(std::move(callback));
}

// static
ManifestManager* ManifestManager::From(LocalFrame& frame) {
  return Supplement<LocalFrame>::From<ManifestManager>(frame);
}

// static
void ManifestManager::ProvideTo(LocalFrame& frame) {
  if (ManifestManager::From(frame))
    return;
  Supplement<LocalFrame>::ProvideTo(
      frame, MakeGarbageCollected<ManifestManager>(frame));
}

ManifestManager::ManifestManager(LocalFrame& frame)
    : Supplement<LocalFrame>(frame),
      ContextLifecycleObserver(frame.GetDocument()),
      may_have_manifest_(false),
      manifest_dirty_(true) {
  if (frame.IsMainFrame()) {
    manifest_change_notifier_ =
        MakeGarbageCollected<ManifestChangeNotifier>(frame);
    frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
        &ManifestManager::BindReceiver, WrapWeakPersistent(this)));
  }
}

ManifestManager::~ManifestManager() = default;

void ManifestManager::RequestManifest(RequestManifestCallback callback) {
  RequestManifestImpl(WTF::Bind(
      [](RequestManifestCallback callback, const KURL& manifest_url,
         const mojom::blink::ManifestPtr& manifest,
         const mojom::blink::ManifestDebugInfo* debug_info) {
        std::move(callback).Run(
            manifest_url, manifest.is_null() ? mojom::blink::Manifest::New()
                                             : manifest->Clone());
      },
      std::move(callback)));
}

void ManifestManager::RequestManifestDebugInfo(
    RequestManifestDebugInfoCallback callback) {
  RequestManifestImpl(WTF::Bind(
      [](RequestManifestDebugInfoCallback callback, const KURL& manifest_url,
         const mojom::blink::ManifestPtr& manifest,
         const mojom::blink::ManifestDebugInfo* debug_info) {
        std::move(callback).Run(manifest_url,
                                debug_info ? debug_info->Clone() : nullptr);
      },
      std::move(callback)));
}

void ManifestManager::RequestManifestForTesting(
    WebManifestManager::Callback callback) {
  RequestManifestImpl(WTF::Bind(
      [](WebManifestManager::Callback callback, const KURL& manifest_url,
         const mojom::blink::ManifestPtr& manifest,
         const mojom::blink::ManifestDebugInfo* debug_info) {
        std::move(callback).Run(manifest_url, manifest.To<Manifest>());
      },
      std::move(callback)));
}

bool ManifestManager::CanFetchManifest() {
  if (!GetSupplementable())
    return false;
  // Do not fetch the manifest if we are on an opaque origin.
  return !GetSupplementable()->GetDocument()->GetSecurityOrigin()->IsOpaque();
}

void ManifestManager::RequestManifestImpl(
    InternalRequestManifestCallback callback) {
  if (!GetSupplementable() || !GetSupplementable()->GetDocument() ||
      !GetSupplementable()->IsAttached()) {
    std::move(callback).Run(KURL(), mojom::blink::ManifestPtr(), nullptr);
    return;
  }

  if (!may_have_manifest_) {
    std::move(callback).Run(KURL(), mojom::blink::ManifestPtr(), nullptr);
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
  manifest_url_ = KURL();
  manifest_debug_info_ = nullptr;
  if (manifest_change_notifier_)
    manifest_change_notifier_->DidChangeManifest();
}

void ManifestManager::DidCommitLoad() {
  may_have_manifest_ = false;
  manifest_dirty_ = true;
  manifest_url_ = KURL();
}

void ManifestManager::FetchManifest() {
  if (!CanFetchManifest()) {
    ManifestUmaUtil::FetchFailed(ManifestUmaUtil::FETCH_FROM_OPAQUE_ORIGIN);
    ResolveCallbacks(ResolveStateFailure);
    return;
  }

  manifest_url_ = ManifestURL();
  if (manifest_url_.IsEmpty()) {
    ManifestUmaUtil::FetchFailed(ManifestUmaUtil::FETCH_EMPTY_URL);
    ResolveCallbacks(ResolveStateFailure);
    return;
  }

  Document& document = *GetSupplementable()->GetDocument();
  fetcher_ = MakeGarbageCollected<ManifestFetcher>(manifest_url_);
  fetcher_->Start(document, ManifestUseCredentials(),
                  WTF::Bind(&ManifestManager::OnManifestFetchComplete,
                            WrapWeakPersistent(this), document.Url()));
}

void ManifestManager::OnManifestFetchComplete(const KURL& document_url,
                                              const ResourceResponse& response,
                                              const String& data) {
  fetcher_ = nullptr;
  if (response.IsNull() && data.IsEmpty()) {
    manifest_debug_info_ = nullptr;
    ManifestUmaUtil::FetchFailed(ManifestUmaUtil::FETCH_UNSPECIFIED_REASON);
    ResolveCallbacks(ResolveStateFailure);
    return;
  }

  ManifestUmaUtil::FetchSucceeded();
  ManifestParser parser(data, response.CurrentRequestUrl(), document_url);
  parser.Parse();

  manifest_debug_info_ = mojom::blink::ManifestDebugInfo::New();
  manifest_debug_info_->raw_manifest = data;
  parser.TakeErrors(&manifest_debug_info_->errors);

  for (const auto& error : manifest_debug_info_->errors) {
    auto location = std::make_unique<SourceLocation>(
        ManifestURL().GetString(), error->line, error->column, nullptr, 0);

    GetSupplementable()->Console().AddMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kOther,
        error->critical ? mojom::ConsoleMessageLevel::kError
                        : mojom::ConsoleMessageLevel::kWarning,
        "Manifest: " + error->message, std::move(location)));
  }

  // Having errors while parsing the manifest doesn't mean the manifest parsing
  // failed. Some properties might have been ignored but some others kept.
  if (parser.failed()) {
    ResolveCallbacks(ResolveStateFailure);
    return;
  }

  manifest_url_ = response.CurrentRequestUrl();
  manifest_ = parser.manifest().Clone();
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
    manifest_ = mojom::blink::ManifestPtr();

  manifest_dirty_ = state != ResolveStateSuccess;

  Vector<InternalRequestManifestCallback> callbacks;
  callbacks.swap(pending_callbacks_);

  for (auto& callback : callbacks) {
    std::move(callback).Run(manifest_url_, manifest_,
                            manifest_debug_info_.get());
  }
}

KURL ManifestManager::ManifestURL() const {
  HTMLLinkElement* link_element =
      GetSupplementable()->GetDocument()->LinkManifest();
  if (!link_element)
    return KURL();
  return link_element->Href();
}

bool ManifestManager::ManifestUseCredentials() const {
  HTMLLinkElement* link_element =
      GetSupplementable()->GetDocument()->LinkManifest();
  if (!link_element)
    return false;
  return EqualIgnoringASCIICase(
      link_element->FastGetAttribute(html_names::kCrossoriginAttr),
      "use-credentials");
}

void ManifestManager::BindReceiver(
    mojo::PendingReceiver<mojom::blink::ManifestManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ManifestManager::ContextDestroyed(ExecutionContext*) {
  if (fetcher_)
    fetcher_->Cancel();

  // Consumers in the browser process will not receive this message but they
  // will be aware of the RenderFrame dying and should act on that. Consumers
  // in the renderer process should be correctly notified.
  ResolveCallbacks(ResolveStateFailure);

  receivers_.Clear();
}

void ManifestManager::Prefinalize() {
  receivers_.Clear();
}

void ManifestManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(fetcher_);
  visitor->Trace(manifest_change_notifier_);
  Supplement<LocalFrame>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
