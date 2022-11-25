// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_manager.h"

#include <utility>

#include "base/bind.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/manifest/manifest_change_notifier.h"
#include "third_party/blink/renderer/modules/manifest/manifest_fetcher.h"
#include "third_party/blink/renderer/modules/manifest/manifest_parser.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

namespace blink {

// static
const char ManifestManager::kSupplementName[] = "ManifestManager";

// static
void WebManifestManager::RequestManifestForTesting(WebLocalFrame* web_frame,
                                                   Callback callback) {
  auto* window = To<WebLocalFrameImpl>(web_frame)->GetFrame()->DomWindow();
  ManifestManager* manifest_manager = ManifestManager::From(*window);
  manifest_manager->RequestManifestForTesting(std::move(callback));
}

// static
ManifestManager* ManifestManager::From(LocalDOMWindow& window) {
  auto* manager = Supplement<LocalDOMWindow>::From<ManifestManager>(window);
  if (!manager) {
    manager = MakeGarbageCollected<ManifestManager>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, manager);
  }
  return manager;
}

ManifestManager::ManifestManager(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      ExecutionContextLifecycleObserver(&window),
      may_have_manifest_(false),
      manifest_dirty_(true),
      receivers_(this, GetExecutionContext()) {
  if (window.GetFrame()->IsMainFrame()) {
    manifest_change_notifier_ =
        MakeGarbageCollected<ManifestChangeNotifier>(window);
    window.GetFrame()->GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
        &ManifestManager::BindReceiver, WrapWeakPersistent(this)));
  }
}

ManifestManager::~ManifestManager() = default;

void ManifestManager::RequestManifest(RequestManifestCallback callback) {
  RequestManifestImpl(WTF::BindOnce(
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
  RequestManifestImpl(WTF::BindOnce(
      [](RequestManifestDebugInfoCallback callback, const KURL& manifest_url,
         const mojom::blink::ManifestPtr& manifest,
         const mojom::blink::ManifestDebugInfo* debug_info) {
        std::move(callback).Run(manifest_url,
                                manifest.is_null()
                                    ? mojom::blink::Manifest::New()
                                    : manifest->Clone(),
                                debug_info ? debug_info->Clone() : nullptr);
      },
      std::move(callback)));
}

void ManifestManager::RequestManifestForTesting(
    WebManifestManager::Callback callback) {
  RequestManifestImpl(WTF::BindOnce(
      [](WebManifestManager::Callback callback, const KURL& manifest_url,
         const mojom::blink::ManifestPtr& manifest,
         const mojom::blink::ManifestDebugInfo* debug_info) {
        std::move(callback).Run(manifest_url);
      },
      std::move(callback)));
}

bool ManifestManager::CanFetchManifest() {
  // Do not fetch the manifest if we are on an opaque origin.
  return !GetSupplementable()->GetSecurityOrigin()->IsOpaque();
}

void ManifestManager::RequestManifestImpl(
    InternalRequestManifestCallback callback) {
  if (!GetSupplementable()->GetFrame()) {
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

void ManifestManager::FetchManifest() {
  if (!CanFetchManifest()) {
    ResolveCallbacks(ResolveState::kFailure);
    return;
  }

  manifest_url_ = ManifestURL();
  if (manifest_url_.IsEmpty()) {
    ResolveCallbacks(ResolveState::kFailure);
    return;
  }

  LocalDOMWindow& window = *GetSupplementable();
  ResourceFetcher* document_fetcher = window.document()->Fetcher();
  fetcher_ = MakeGarbageCollected<ManifestFetcher>(manifest_url_);
  fetcher_->Start(window, ManifestUseCredentials(), document_fetcher,
                  WTF::BindOnce(&ManifestManager::OnManifestFetchComplete,
                                WrapWeakPersistent(this), window.Url()));
}

void ManifestManager::OnManifestFetchComplete(const KURL& document_url,
                                              const ResourceResponse& response,
                                              const String& data) {
  fetcher_ = nullptr;
  if (response.IsNull() && data.empty()) {
    manifest_debug_info_ = nullptr;
    ResolveCallbacks(ResolveState::kFailure);
    return;
  }

  // We are using the document as our FeatureContext for checking origin trials.
  // Note that any origin trials delivered in the manifest HTTP headers will be
  // ignored, only ones associated with the page will be used.
  ManifestParser parser(data, response.CurrentRequestUrl(), document_url,
                        GetExecutionContext());

  // Monitoring whether the manifest has comments is temporary. Once
  // warning/deprecation period is over, we should remove this as it's
  // technically incorrect JSON syntax anyway. See crbug.com/1264024
  bool has_comments = parser.Parse();
  if (has_comments) {
    UseCounter::Count(GetSupplementable(),
                      WebFeature::kWebAppManifestHasComments);
  }

  manifest_debug_info_ = mojom::blink::ManifestDebugInfo::New();
  manifest_debug_info_->raw_manifest = data.IsNull() ? "" : data;
  parser.TakeErrors(&manifest_debug_info_->errors);

  for (const auto& error : manifest_debug_info_->errors) {
    auto location = std::make_unique<SourceLocation>(ManifestURL().GetString(),
                                                     String(), error->line,
                                                     error->column, nullptr, 0);

    GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        error->critical ? mojom::blink::ConsoleMessageLevel::kError
                        : mojom::blink::ConsoleMessageLevel::kWarning,
        "Manifest: " + error->message, std::move(location)));
  }

  // Having errors while parsing the manifest doesn't mean the manifest parsing
  // failed. Some properties might have been ignored but some others kept.
  if (parser.failed()) {
    ResolveCallbacks(ResolveState::kFailure);
    return;
  }

  manifest_url_ = response.CurrentRequestUrl();
  manifest_ = parser.manifest().Clone();
  RecordMetrics(*manifest_);
  ResolveCallbacks(ResolveState::kSuccess);
}

void ManifestManager::RecordMetrics(const mojom::blink::Manifest& manifest) {
  if (manifest.capture_links != mojom::blink::CaptureLinks::kUndefined) {
    UseCounter::Count(GetSupplementable(),
                      WebFeature::kWebAppManifestCaptureLinks);
  }

  if (!manifest.launch_handler.is_null()) {
    UseCounter::Count(GetSupplementable(),
                      WebFeature::kWebAppManifestLaunchHandler);
  }

  if (!manifest.url_handlers.empty()) {
    UseCounter::Count(GetSupplementable(),
                      WebFeature::kWebAppManifestUrlHandlers);
  }

  if (!manifest.protocol_handlers.empty()) {
    UseCounter::Count(GetSupplementable(),
                      WebFeature::kWebAppManifestProtocolHandlers);
  }

  for (const mojom::blink::DisplayMode& display_override :
       manifest.display_override) {
    if (display_override == mojom::blink::DisplayMode::kWindowControlsOverlay) {
      UseCounter::Count(GetSupplementable(),
                        WebFeature::kWebAppWindowControlsOverlay);
    } else if (display_override == mojom::blink::DisplayMode::kBorderless) {
      UseCounter::Count(GetSupplementable(), WebFeature::kWebAppBorderless);
    }
  }

  if (manifest.has_dark_theme_color || manifest.has_dark_background_color) {
    UseCounter::Count(GetSupplementable(),
                      WebFeature::kWebAppManifestUserPreferences);
  }
}

void ManifestManager::ResolveCallbacks(ResolveState state) {
  // Do not reset |manifest_url_| on failure here. If manifest_url_ is
  // non-empty, that means the link 404s, we failed to fetch it, or it was
  // unparseable. However, the site still tried to specify a manifest, so
  // preserve that information in the URL for the callbacks.
  // |manifest_url| will be reset on navigation or if we receive a didchange
  // event.
  if (state == ResolveState::kFailure)
    manifest_ = mojom::blink::ManifestPtr();

  manifest_dirty_ = state != ResolveState::kSuccess;

  Vector<InternalRequestManifestCallback> callbacks;
  callbacks.swap(pending_callbacks_);

  for (auto& callback : callbacks) {
    std::move(callback).Run(manifest_url_, manifest_,
                            manifest_debug_info_.get());
  }
}

KURL ManifestManager::ManifestURL() const {
  HTMLLinkElement* link_element =
      GetSupplementable()->document()->LinkManifest();
  if (!link_element)
    return KURL();
  return link_element->Href();
}

bool ManifestManager::ManifestUseCredentials() const {
  HTMLLinkElement* link_element =
      GetSupplementable()->document()->LinkManifest();
  if (!link_element)
    return false;
  return EqualIgnoringASCIICase(
      link_element->FastGetAttribute(html_names::kCrossoriginAttr),
      "use-credentials");
}

void ManifestManager::BindReceiver(
    mojo::PendingReceiver<mojom::blink::ManifestManager> receiver) {
  receivers_.Add(std::move(receiver),
                 GetSupplementable()->GetTaskRunner(TaskType::kNetworking));
}

void ManifestManager::ContextDestroyed() {
  if (fetcher_)
    fetcher_->Cancel();

  // Consumers in the browser process will not receive this message but they
  // will be aware of the RenderFrame dying and should act on that. Consumers
  // in the renderer process should be correctly notified.
  ResolveCallbacks(ResolveState::kFailure);
}

void ManifestManager::Trace(Visitor* visitor) const {
  visitor->Trace(fetcher_);
  visitor->Trace(manifest_change_notifier_);
  visitor->Trace(receivers_);
  Supplement<LocalDOMWindow>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
