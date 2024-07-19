// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_manager.h"

#include <utility>

#include "base/functional/bind.h"
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
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

ManifestManager::Result::Result(mojom::blink::ManifestRequestResult result,
                                KURL manifest_url,
                                mojom::blink::ManifestPtr manifest)
    : result_(result),
      manifest_url_(manifest_url),
      manifest_(manifest ? std::move(manifest) : mojom::blink::Manifest::New()),
      debug_info_(mojom::blink::ManifestDebugInfo::New()) {
  // The default constructor for ManifestDebugInfo does not initialize
  // `raw_manifest` with a valid value, so do so here instead.
  debug_info_->raw_manifest = "";
}

ManifestManager::Result::Result(Result&&) = default;
ManifestManager::Result& ManifestManager::Result::operator=(Result&&) = default;

void ManifestManager::Result::SetManifest(mojom::blink::ManifestPtr manifest) {
  CHECK(manifest);
  manifest_ = std::move(manifest);
}

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
      [](RequestManifestCallback callback, const Result& result) {
        std::move(callback).Run(result.result(), result.manifest_url(),
                                result.manifest().Clone());
      },
      std::move(callback)));
}

void ManifestManager::RequestManifestDebugInfo(
    RequestManifestDebugInfoCallback callback) {
  RequestManifestImpl(WTF::BindOnce(
      [](RequestManifestDebugInfoCallback callback, const Result& result) {
        std::move(callback).Run(result.manifest_url(),
                                result.manifest().Clone(),
                                result.debug_info().Clone());
      },
      std::move(callback)));
}

void ManifestManager::ParseManifestFromString(
    const KURL& document_url,
    const KURL& manifest_url,
    const String& manifest_contents,
    ParseManifestFromStringCallback callback) {
  ManifestParser parser(manifest_contents, manifest_url, document_url,
                        GetExecutionContext());
  parser.Parse();

  mojom::blink::ManifestPtr result;
  if (!parser.failed()) {
    result = parser.TakeManifest();
  }

  std::move(callback).Run(std::move(result));
}

void ManifestManager::RequestManifestForTesting(
    WebManifestManager::Callback callback) {
  RequestManifestImpl(WTF::BindOnce(
      [](WebManifestManager::Callback callback, const Result& result) {
        std::move(callback).Run(result.manifest_url());
      },
      std::move(callback)));
}

bool ManifestManager::CanFetchManifest() {
  // Do not fetch the manifest if we are on an opaque origin.
  return !GetSupplementable()->GetSecurityOrigin()->IsOpaque() &&
         GetSupplementable()->Url().IsValid();
}

void ManifestManager::RequestManifestImpl(
    InternalRequestManifestCallback callback) {
  if (!GetSupplementable()->GetFrame()) {
    std::move(callback).Run(
        Result(mojom::blink::ManifestRequestResult::kUnexpectedFailure));
    return;
  }

  if (cached_result_) {
    std::move(callback).Run(*cached_result_);
    return;
  }

  pending_callbacks_.push_back(std::move(callback));

  // Just wait for the running call to be done if there are other callbacks.
  if (pending_callbacks_.size() > 1)
    return;

  FetchManifest();
}

void ManifestManager::DidChangeManifest() {
  cached_result_.reset();
  if (manifest_change_notifier_) {
    manifest_change_notifier_->DidChangeManifest();
  }
}

void ManifestManager::FetchManifest() {
  if (!CanFetchManifest()) {
    ResolveCallbacks(
        Result(mojom::blink::ManifestRequestResult::kNoManifestAllowed,
               ManifestURL()));
    return;
  }

  LocalDOMWindow& window = *GetSupplementable();
  KURL manifest_url = ManifestURL();
  if (manifest_url.IsEmpty()) {
    ResolveCallbacks(
        Result(mojom::blink::ManifestRequestResult::kNoManifestSpecified,
               KURL(), DefaultManifest()));
    return;
  }

  ResourceFetcher* document_fetcher = window.document()->Fetcher();
  fetcher_ = MakeGarbageCollected<ManifestFetcher>(manifest_url);
  fetcher_->Start(window, ManifestUseCredentials(), document_fetcher,
                  WTF::BindOnce(&ManifestManager::OnManifestFetchComplete,
                                WrapWeakPersistent(this), window.Url()));
}

void ManifestManager::OnManifestFetchComplete(const KURL& document_url,
                                              const ResourceResponse& response,
                                              const String& data) {
  fetcher_ = nullptr;
  if (response.IsNull() && data.empty()) {
    // The only time we don't produce the default manifest is when there is a
    // resource fetching problem of the manifest link. This allows callers to
    // catch this error appropriately as a network issue instead of using a
    // 'default' manifest that wasn't intended by the developer.
    ResolveCallbacks(
        Result(mojom::blink::ManifestRequestResult::kManifestFailedToFetch,
               response.CurrentRequestUrl(), DefaultManifest()));
    return;
  }
  ParseManifestFromPage(document_url, response.CurrentRequestUrl(), data);
}

void ManifestManager::ParseManifestFromPage(const KURL& document_url,
                                            std::optional<KURL> manifest_url,
                                            const String& data) {
  CHECK(document_url.IsValid());
  // We are using the document as our FeatureContext for checking origin trials.
  // Note that any origin trials delivered in the manifest HTTP headers will be
  // ignored, only ones associated with the page will be used.
  // For default manifests, the manifest_url is `std::nullopt`, so use the
  // document_url instead for the parsing algorithm.
  ManifestParser parser(data, manifest_url.value_or(document_url), document_url,
                        GetExecutionContext());

  // Monitoring whether the manifest has comments is temporary. Once
  // warning/deprecation period is over, we should remove this as it's
  // technically incorrect JSON syntax anyway. See crbug.com/1264024
  bool has_comments = parser.Parse();
  if (has_comments) {
    UseCounter::Count(GetSupplementable(),
                      WebFeature::kWebAppManifestHasComments);
  }

  const bool failed = parser.failed();
  Result result(
      failed ? mojom::blink::ManifestRequestResult::kManifestFailedToParse
             : mojom::blink::ManifestRequestResult::kSuccess,
      manifest_url.value_or(KURL()));

  result.debug_info().raw_manifest = data.IsNull() ? "" : data;
  parser.TakeErrors(&result.debug_info().errors);

  for (const auto& error : result.debug_info().errors) {
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
  if (failed) {
    result.SetManifest(DefaultManifest());
    ResolveCallbacks(std::move(result));
    return;
  }

  result.SetManifest(parser.TakeManifest());

  // We should always have a start_url, manifest_id, and scope, as any errors
  // still have fallbacks back to the document_url.
  CHECK(!result.manifest().start_url.IsEmpty() &&
        result.manifest().start_url.IsValid());
  CHECK(!result.manifest().id.IsEmpty() && result.manifest().id.IsValid());
  CHECK(!result.manifest().scope.IsEmpty() &&
        result.manifest().scope.IsValid());

  RecordMetrics(result.manifest());
  ResolveCallbacks(std::move(result));
}

void ManifestManager::RecordMetrics(const mojom::blink::Manifest& manifest) {
  if (manifest.has_custom_id) {
    UseCounter::Count(GetSupplementable(), WebFeature::kWebAppManifestIdField);
  }

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

  if (!manifest.scope_extensions.empty()) {
    UseCounter::Count(GetSupplementable(),
                      WebFeature::kWebAppManifestScopeExtensions);
  }

  for (const mojom::blink::DisplayMode& display_override :
       manifest.display_override) {
    if (display_override == mojom::blink::DisplayMode::kWindowControlsOverlay) {
      UseCounter::Count(GetSupplementable(),
                        WebFeature::kWebAppWindowControlsOverlay);
    } else if (display_override == mojom::blink::DisplayMode::kBorderless) {
      UseCounter::Count(GetSupplementable(), WebFeature::kWebAppBorderless);
    } else if (display_override == mojom::blink::DisplayMode::kTabbed) {
      UseCounter::Count(GetSupplementable(), WebFeature::kWebAppTabbed);
    }
  }
}

void ManifestManager::ResolveCallbacks(Result result) {
  Vector<InternalRequestManifestCallback> callbacks;
  callbacks.swap(pending_callbacks_);

  // URLs that are too long are silently truncated by the mojo serialization.
  // Since that might violate invariants the manifest is expected to have, check
  // if any URLs would be too long and return an error instead if that is the
  // case.
  const bool has_overlong_urls =
      result.manifest().manifest_url.GetString().length() > url::kMaxURLChars ||
      result.manifest().id.GetString().length() > url::kMaxURLChars ||
      result.manifest().start_url.GetString().length() > url::kMaxURLChars ||
      result.manifest().scope.GetString().length() > url::kMaxURLChars;
  if (has_overlong_urls) {
    result = Result(mojom::blink::ManifestRequestResult::kUnexpectedFailure);
  }

  const Result* result_ptr = nullptr;
  if (result.result() == mojom::blink::ManifestRequestResult::kSuccess) {
    cached_result_ = std::move(result);
    result_ptr = &cached_result_.value();
  } else {
    result_ptr = &result;
  }

  for (auto& callback : callbacks) {
    std::move(callback).Run(*result_ptr);
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

mojom::blink::ManifestPtr ManifestManager::DefaultManifest() {
  // Generate the default manifest for failures, and use the current window url
  // as the manifest_url for resolving resources in the default manifest.
  LocalDOMWindow& window = *GetSupplementable();
  ManifestParser parser(/*data=*/"{ }", /*manifest_url=*/window.Url(),
                        /*document_url=*/window.Url(), GetExecutionContext());
  parser.Parse();
  CHECK(!parser.failed());
  auto result = parser.TakeManifest();
  // Reset manifest_url in the parsed manifest, as the window url isn't really
  // the url for this manifest.
  result->manifest_url = KURL();
  return result;
}

void ManifestManager::ContextDestroyed() {
  if (fetcher_)
    fetcher_->Cancel();

  // Consumers in the browser process will not receive this message but they
  // will be aware of the RenderFrame dying and should act on that. Consumers
  // in the renderer process should be correctly notified.
  ResolveCallbacks(
      Result(mojom::blink::ManifestRequestResult::kUnexpectedFailure));
}

void ManifestManager::Trace(Visitor* visitor) const {
  visitor->Trace(fetcher_);
  visitor->Trace(manifest_change_notifier_);
  visitor->Trace(receivers_);
  Supplement<LocalDOMWindow>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
