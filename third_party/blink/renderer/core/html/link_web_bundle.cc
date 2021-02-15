// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/link_web_bundle.h"

#include "base/unguessable_token.h"
#include "services/network/public/mojom/web_bundle_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class WebBundleLoader : public GarbageCollected<WebBundleLoader>,
                        public ThreadableLoaderClient,
                        public network::mojom::WebBundleHandle {
 public:
  WebBundleLoader(LinkWebBundle& link_web_bundle,
                  Document& document,
                  const KURL& url,
                  CrossOriginAttributeValue cross_origin_attribute_value)
      : link_web_bundle_(&link_web_bundle),
        url_(url),
        security_origin_(SecurityOrigin::Create(url)),
        web_bundle_token_(base::UnguessableToken::Create()) {
    ResourceRequest request(url);
    request.SetUseStreamOnResponse(true);
    // TODO(crbug.com/1082020): Revisit these once the fetch and process the
    // linked resource algorithm [1] for <link rel=webbundle> is defined.
    // [1]
    // https://html.spec.whatwg.org/multipage/semantics.html#fetch-and-process-the-linked-resource
    request.SetRequestContext(
        mojom::blink::RequestContextType::SUBRESOURCE_WEBBUNDLE);
    // Set request's mode and credentials mode. See
    // https://html.spec.whatwg.org/multipage/#cors-settings-attribute
    switch (cross_origin_attribute_value) {
      case kCrossOriginAttributeNotSet:
        request.SetMode(network::mojom::blink::RequestMode::kNoCors);
        request.SetCredentialsMode(network::mojom::CredentialsMode::kInclude);
        break;
      case kCrossOriginAttributeAnonymous:
        request.SetMode(network::mojom::blink::RequestMode::kCors);
        request.SetCredentialsMode(
            network::mojom::CredentialsMode::kSameOrigin);
        break;
      case kCrossOriginAttributeUseCredentials:
        request.SetMode(network::mojom::blink::RequestMode::kCors);
        request.SetCredentialsMode(network::mojom::CredentialsMode::kInclude);
        break;
    }
    request.SetRequestDestination(
        network::mojom::RequestDestination::kWebBundle);
    request.SetPriority(ResourceLoadPriority::kHigh);

    mojo::PendingRemote<network::mojom::WebBundleHandle> web_bundle_handle;
    web_bundle_handles_.Add(this,
                            web_bundle_handle.InitWithNewPipeAndPassReceiver());
    request.SetWebBundleTokenParams(ResourceRequestHead::WebBundleTokenParams(
        web_bundle_token_, std::move(web_bundle_handle)));

    ExecutionContext* execution_context = document.GetExecutionContext();
    ResourceLoaderOptions resource_loader_options(
        execution_context->GetCurrentWorld());
    resource_loader_options.data_buffering_policy = kDoNotBufferData;

    loader_ = MakeGarbageCollected<ThreadableLoader>(*execution_context, this,
                                                     resource_loader_options);
    loader_->Start(std::move(request));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(link_web_bundle_);
    visitor->Trace(loader_);
  }

  bool HasLoaded() const { return !failed_; }

  // ThreadableLoaderClient
  void DidReceiveResponse(uint64_t, const ResourceResponse& response) override {
    if (!cors::IsOkStatus(response.HttpStatusCode()))
      failed_ = true;
  }

  void DidStartLoadingResponseBody(BytesConsumer& consumer) override {
    // Drain |consumer| so that DidFinishLoading is surely called later.
    consumer.DrainAsDataPipe();
  }

  void DidFinishLoading(uint64_t) override { link_web_bundle_->NotifyLoaded(); }
  void DidFail(const ResourceError&) override { DidFailInternal(); }
  void DidFailRedirectCheck() override { DidFailInternal(); }

  // network::mojom::WebBundleHandle
  void Clone(mojo::PendingReceiver<network::mojom::WebBundleHandle> receiver)
      override {
    web_bundle_handles_.Add(this, std::move(receiver));
  }
  void OnWebBundleError(network::mojom::WebBundleErrorType type,
                        const std::string& message) override {
    link_web_bundle_->OnWebBundleError(url_.ElidedString() + ": " +
                                       message.c_str());
  }

  const KURL& url() const { return url_; }
  scoped_refptr<SecurityOrigin> GetSecurityOrigin() const {
    return security_origin_;
  }
  const base::UnguessableToken& WebBundleToken() const {
    return web_bundle_token_;
  }

 private:
  void DidFailInternal() {
    failed_ = true;
    link_web_bundle_->NotifyLoaded();
  }

  Member<LinkWebBundle> link_web_bundle_;
  Member<ThreadableLoader> loader_;
  bool failed_ = false;
  KURL url_;
  scoped_refptr<SecurityOrigin> security_origin_;
  base::UnguessableToken web_bundle_token_;
  // we need ReceiverSet here because WebBundleHandle is cloned when
  // ResourceRequest is copied.
  mojo::ReceiverSet<network::mojom::WebBundleHandle> web_bundle_handles_;
};

LinkWebBundle::LinkWebBundle(HTMLLinkElement* owner) : LinkResource(owner) {
  UseCounter::Count(owner_->GetDocument().GetExecutionContext(),
                    WebFeature::kSubresourceWebBundles);
}
LinkWebBundle::~LinkWebBundle() = default;

void LinkWebBundle::Trace(Visitor* visitor) const {
  visitor->Trace(bundle_loader_);
  LinkResource::Trace(visitor);
  SubresourceWebBundle::Trace(visitor);
}

void LinkWebBundle::NotifyLoaded() {
  if (owner_)
    owner_->ScheduleEvent();
}

void LinkWebBundle::OnWebBundleError(const String& message) const {
  if (!owner_)
    return;
  ExecutionContext* context = owner_->GetDocument().GetExecutionContext();
  if (!context)
    return;
  context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kOther,
      mojom::blink::ConsoleMessageLevel::kWarning, message));
}

void LinkWebBundle::Process() {
  if (!owner_ || !owner_->GetDocument().GetFrame())
    return;
  if (!owner_->ShouldLoadLink())
    return;

  ResourceFetcher* resource_fetcher = owner_->GetDocument().Fetcher();
  if (!resource_fetcher)
    return;

  // We don't support crossorigin= attribute's dynamic change. It seems
  // other types of link elements doesn't support that too. See
  // HTMLlinkElement::ParseAttribute, which doesn't call Process() for
  // crossorigin= attribute change.
  if (!bundle_loader_ || bundle_loader_->url() != owner_->Href()) {
    if (resource_fetcher->ShouldBeLoadedFromWebBundle(owner_->Href())) {
      // This can happen when a requested bundle is a nested bundle.
      //
      // clang-format off
      // Example:
      // <link rel="webbundle" href=".../nested-main.wbn" resources=".../nested-sub.wbn">
      // <link rel="webbundle" href=".../nested-sub.wbn" resources="...">
      // clang-format on
      if (bundle_loader_) {
        resource_fetcher->RemoveSubresourceWebBundle(*this);
        bundle_loader_ = nullptr;
      }
      NotifyLoaded();
      OnWebBundleError("A nested bundle is not supported: " +
                       owner_->Href().ElidedString());
      return;
    }
    bundle_loader_ = MakeGarbageCollected<WebBundleLoader>(
        *this, owner_->GetDocument(), owner_->Href(),
        GetCrossOriginAttributeValue(
            owner_->FastGetAttribute(html_names::kCrossoriginAttr)));
  }

  resource_fetcher->AddSubresourceWebBundle(*this);
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
  resource_fetcher->RemoveSubresourceWebBundle(*this);
  bundle_loader_ = nullptr;
}

bool LinkWebBundle::CanHandleRequest(const KURL& url) const {
  if (!url.IsValid())
    return false;
  if (!ResourcesOrScopesMatch(url))
    return false;
  if (url.Protocol() == "urn")
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

const base::UnguessableToken& LinkWebBundle::WebBundleToken() const {
  DCHECK(bundle_loader_);
  return bundle_loader_->WebBundleToken();
}

// static
KURL LinkWebBundle::ParseResourceUrl(const AtomicString& str) {
  // The implementation is almost copy and paste from ParseExchangeURL() defined
  // in services/data_decoder/web_bundle_parser.cc, replacing GURL with KURL.

  // TODO(hayato): Consider to support a relative URL.
  KURL url(str);
  if (!url.IsValid())
    return KURL();

  // Exchange URL must not have a fragment or credentials.
  if (url.HasFragmentIdentifier() || !url.User().IsEmpty() ||
      !url.Pass().IsEmpty())
    return KURL();

  // For now, we allow only http:, https: and urn: schemes in Web Bundle URLs.
  // TODO(crbug.com/966753): Revisit this once
  // https://github.com/WICG/webpackage/issues/468 is resolved.
  if (!url.ProtocolIsInHTTPFamily() && !url.ProtocolIs("urn"))
    return KURL();

  return url;
}

}  // namespace blink
