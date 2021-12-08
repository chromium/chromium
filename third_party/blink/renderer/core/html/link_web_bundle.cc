// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/link_web_bundle.h"

#include "base/notreached.h"
#include "base/unguessable_token.h"
#include "services/network/public/mojom/web_bundle_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/core/loader/web_bundle/web_bundle_loader.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/subresource_web_bundle_list.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver_set.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

// static
bool LinkWebBundle::IsFeatureEnabled(const ExecutionContext* context) {
  return context && context->IsSecureContext() &&
         RuntimeEnabledFeatures::SubresourceWebBundlesEnabled(context);
}

LinkWebBundle::LinkWebBundle(HTMLLinkElement* owner) : LinkResource(owner) {
  UseCounter::Count(owner_->GetDocument().GetExecutionContext(),
                    WebFeature::kSubresourceWebBundles);
  AddConsoleMessage(
      "<link rel=\"webbundle\"> is deprecated. See migration guide at "
      "https://bit.ly/3rpDuEX.");
}
LinkWebBundle::~LinkWebBundle() = default;

void LinkWebBundle::Trace(Visitor* visitor) const {
  visitor->Trace(bundle_loader_);
  LinkResource::Trace(visitor);
  SubresourceWebBundle::Trace(visitor);
}

void LinkWebBundle::NotifyLoadingFinished() {
  if (owner_)
    owner_->ScheduleEvent();
}

void LinkWebBundle::OnWebBundleError(const String& message) const {
  AddConsoleMessage(message);
}

void LinkWebBundle::AddConsoleMessage(const String& message) const {
  if (!owner_)
    return;
  ExecutionContext* context = owner_->GetDocument().GetExecutionContext();
  if (!context)
    return;
  context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kOther,
      mojom::blink::ConsoleMessageLevel::kWarning, message));
}

bool LinkWebBundle::IsScriptWebBundle() const {
  NOTREACHED() << "Should never happen since IsScriptWebBundle() is called "
                  "only for ScriptWebBundle in the current implementation.";
  return false;
}

bool LinkWebBundle::WillBeReleased() const {
  return false;
}

network::mojom::CredentialsMode LinkWebBundle::GetCredentialsMode() const {
  NOTREACHED() << "Should never happen since GetCredentialsMode() is called "
                  "only for ScriptWebBundle in the current implementation.";
  return network::mojom::CredentialsMode::kOmit;
}

namespace {

network::mojom::CredentialsMode BundleRequestCredentialsMode(
    CrossOriginAttributeValue attr) {
  switch (attr) {
    case kCrossOriginAttributeNotSet:
    case kCrossOriginAttributeAnonymous:
      return network::mojom::CredentialsMode::kSameOrigin;
    case kCrossOriginAttributeUseCredentials:
      return network::mojom::CredentialsMode::kInclude;
  }
}

}  // namespace

void LinkWebBundle::Process() {
  if (!owner_ || !owner_->GetDocument().GetFrame())
    return;
  if (!owner_->ShouldLoadLink())
    return;

  ResourceFetcher* resource_fetcher = owner_->GetDocument().Fetcher();
  if (!resource_fetcher)
    return;
  SubresourceWebBundleList* active_bundles =
      resource_fetcher->GetOrCreateSubresourceWebBundleList();

  // We don't support crossorigin= attribute's dynamic change. It seems
  // other types of link elements doesn't support that too. See
  // HTMLlinkElement::ParseAttribute, which doesn't call Process() for
  // crossorigin= attribute change.
  if (!bundle_loader_ || bundle_loader_->url() != owner_->Href()) {
    if (active_bundles->GetMatchingBundle(owner_->Href())) {
      // This can happen when a requested bundle is a nested bundle.
      //
      // clang-format off
      // Example:
      // <link rel="webbundle" href=".../nested-main.wbn" resources=".../nested-sub.wbn">
      // <link rel="webbundle" href=".../nested-sub.wbn" resources="...">
      // clang-format on
      if (bundle_loader_) {
        active_bundles->Remove(*this);
        ReleaseBundleLoader();
      }
      NotifyLoadingFinished();
      OnWebBundleError("A nested bundle is not supported: " +
                       owner_->Href().ElidedString());
      return;
    }
    // Release the old resources explicitly before assigning new bundle loader
    // instead of relying on GC.
    if (bundle_loader_) {
      active_bundles->Remove(*this);
      ReleaseBundleLoader();
    }

    bundle_loader_ = MakeGarbageCollected<WebBundleLoader>(
        *this, owner_->GetDocument(), owner_->Href(),
        BundleRequestCredentialsMode(GetCrossOriginAttributeValue(
            owner_->FastGetAttribute(html_names::kCrossoriginAttr))));
  }

  active_bundles->Add(*this);
}

LinkResource::LinkResourceType LinkWebBundle::GetType() const {
  return kOther;
}

bool LinkWebBundle::HasLoaded() const {
  return bundle_loader_ && bundle_loader_->HasLoaded();
}

void LinkWebBundle::OwnerRemoved() {
  if (!owner_)
    return;
  ResourceFetcher* resource_fetcher = owner_->GetDocument().Fetcher();
  if (!resource_fetcher)
    return;
  SubresourceWebBundleList* active_bundles =
      resource_fetcher->GetOrCreateSubresourceWebBundleList();
  active_bundles->Remove(*this);
  if (bundle_loader_)
    ReleaseBundleLoader();
}

bool LinkWebBundle::CanHandleRequest(const KURL& url) const {
  if (!url.IsValid())
    return false;
  if (!ResourcesOrScopesMatch(url))
    return false;
  // TODO(https://crbug.com/1257045): Remove urn: scheme support.
  if (url.Protocol() == "urn" || url.Protocol() == "uuid-in-package")
    return true;
  DCHECK(bundle_loader_);
  if (!bundle_loader_->GetSecurityOrigin()->IsSameOriginWith(
          SecurityOrigin::Create(url).get())) {
    OnWebBundleError(url.ElidedString() + " cannot be loaded from WebBundle " +
                     bundle_loader_->url().ElidedString() +
                     ": bundled resource must be same origin with the bundle.");
    return false;
  }

  if (!url.GetString().StartsWith(bundle_loader_->url().BaseAsString())) {
    OnWebBundleError(
        url.ElidedString() + " cannot be loaded from WebBundle " +
        bundle_loader_->url().ElidedString() +
        ": bundled resource path must contain the bundle's path as a prefix.");
    return false;
  }
  return true;
}

bool LinkWebBundle::ResourcesOrScopesMatch(const KURL& url) const {
  if (!owner_)
    return false;
  if (owner_->ValidResourceUrls().Contains(url))
    return true;
  for (const auto& scope : owner_->ValidScopeUrls()) {
    if (url.GetString().StartsWith(scope.GetString()))
      return true;
  }
  return false;
}

String LinkWebBundle::GetCacheIdentifier() const {
  DCHECK(bundle_loader_);
  return bundle_loader_->url().GetString();
}

const KURL& LinkWebBundle::GetBundleUrl() const {
  DCHECK(bundle_loader_);
  return bundle_loader_->url();
}

const base::UnguessableToken& LinkWebBundle::WebBundleToken() const {
  DCHECK(bundle_loader_);
  return bundle_loader_->WebBundleToken();
}

void LinkWebBundle::ReleaseBundleLoader() {
  DCHECK(bundle_loader_);
  // Clear receivers explicitly here, instead of waiting for Blink GC.
  bundle_loader_->ClearReceivers();
  bundle_loader_ = nullptr;
}

// static
KURL LinkWebBundle::CompleteURL(const KURL& base_url, const String& str) {
  if (str.IsNull())
    return KURL();
  return KURL(base_url, str);
}

// static
KURL LinkWebBundle::ParseResourceUrl(const AtomicString& str,
                                     CompleteURLCallback callback) {
  // The implementation is almost copy and paste from ParseExchangeURL() defined
  // in services/data_decoder/web_bundle_parser.cc, replacing GURL with KURL.

  KURL url = callback.Run(str);
  if (!url.IsValid()) {
    return KURL();
  }

  // Exchange URL must not have a fragment or credentials.
  if (url.HasFragmentIdentifier() || !url.User().IsEmpty() ||
      !url.Pass().IsEmpty())
    return KURL();

  // For now, we allow only http:, https:, urn: and uuid-in-package: schemes in
  // Web Bundle URLs.
  // TODO(crbug.com/966753): Revisit this once
  // https://github.com/WICG/webpackage/issues/468 is resolved.
  // TODO(https://crbug.com/1257045): Remove urn: scheme support.
  if (!url.ProtocolIsInHTTPFamily() &&
      !(url.ProtocolIs("urn") || url.ProtocolIs("uuid-in-package")))
    return KURL();

  return url;
}

}  // namespace blink
