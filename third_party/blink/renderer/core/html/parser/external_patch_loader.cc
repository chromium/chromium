// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/external_patch_loader.h"

#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/patch.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

ExternalPatchLoader::ExternalPatchLoader(Patch* owner,
                                         HTMLTemplateElement* template_element,
                                         const AtomicString& src_attr)
    : owner_(owner), template_(template_element) {
  CHECK(RuntimeEnabledFeatures::DeclarativeFragmentEnabled());
  CHECK(!src_attr.empty());

  KURL url = template_->GetDocument().CompleteURL(src_attr);
  if (!url.IsValid()) {
    return;
  }

  ExecutionContext* context = template_->GetDocument().GetExecutionContext();
  if (!context) {
    return;
  }

  ResourceRequest resource_request(url);
  // This fetches fragments as scripts in terms of CSP. We can look later at
  // relaxing it for sanitized fragments.
  resource_request.SetRequestContext(mojom::blink::RequestContextType::SCRIPT);
  resource_request.SetRequestDestination(
      network::mojom::RequestDestination::kScript);

  CrossOriginAttributeValue cross_origin = GetCrossOriginAttributeValue(
      template_->FastGetAttribute(html_names::kCrossoriginAttr));
  const AtomicString& integrity_attr =
      template_->FastGetAttribute(html_names::kIntegrityAttr);
  const AtomicString& referrerpolicy_attr =
      template_->FastGetAttribute(html_names::kReferrerpolicyAttr);

  if (!referrerpolicy_attr.empty()) {
    network::mojom::ReferrerPolicy policy =
        network::mojom::ReferrerPolicy::kDefault;
    if (SecurityPolicy::ReferrerPolicyFromString(
            referrerpolicy_attr, kDoNotSupportReferrerPolicyLegacyKeywords,
            &policy)) {
      resource_request.SetReferrerPolicy(policy);
    }
  }

  FetchParameters params(std::move(resource_request),
                         ResourceLoaderOptions(context->GetCurrentWorld()));

  if (cross_origin == kCrossOriginAttributeNotSet) {
    cross_origin = kCrossOriginAttributeAnonymous;
  }
  params.SetCrossOriginAccessControl(context->GetSecurityOrigin(),
                                     cross_origin);

  if (!integrity_attr.empty()) {
    IntegrityMetadataSet integrity_metadata;
    SubresourceIntegrity::ParseIntegrityAttribute(integrity_attr,
                                                  integrity_metadata, context);
    params.SetIntegrityMetadata(integrity_metadata);
  }

  if (!template_->nonce().empty()) {
    params.SetContentSecurityPolicyNonce(template_->nonce());
  }

  ready_state_ = kWaitingForResource;
  resource_ =
      RawResource::Fetch(params, template_->GetDocument().Fetcher(), this);
}

void ExternalPatchLoader::Cancel() {
  if (resource_) {
    resource_->RemoveClient(this);
    resource_ = nullptr;
  }
  if (parser_) {
    static_cast<DocumentParser*>(parser_.Get())->Finish();
    parser_ = nullptr;
  }
  ready_state_ = kReady;
}

void ExternalPatchLoader::ResponseReceived(Resource* resource,
                                           const ResourceResponse& response) {
  CHECK(RuntimeEnabledFeatures::DeclarativeFragmentEnabled());
  if (response.HttpStatusCode() != 200 || response.MimeType() != "text/html") {
    ready_state_ = kErrorOccurred;
    return;
  }

  DocumentFragment* fragment = nullptr;
  if (owner_->is_buffered()) {
    fragment = template_->content();
    fragment->RemoveChildren();
  } else {
    fragment = DocumentFragment::Create(template_->GetDocument());
  }

  ContainerNode* target = owner_->is_buffered() ? fragment : owner_->parent();
  ParserRootInsertionPoint* root_insertion_point = nullptr;

  if (!owner_->is_buffered()) {
    // For targeted streaming, parse directly into target scope
    // Use end marker as the anchor node for insertion
    root_insertion_point = MakeGarbageCollected<ParserRootInsertionPoint>(
        *target, owner_->end_marker());
  }

  Element* context_element = DynamicTo<Element>(target);
  if (!context_element) {
    context_element = template_;
  }

  ParserContentPolicy content_policy =
      owner_->is_buffered()
          ? ParserContentPolicy::kDisallowScriptingAndPluginContent
          : ParserContentPolicy::kAllowScriptingContentAndMarkAsParserInserted;

  parser_ = MakeGarbageCollected<HTMLDocumentParser>(
      fragment, context_element, content_policy,
      ParserPrefetchPolicy::kDisallowPrefetching, nullptr, nullptr,
      root_insertion_point);

  parser_->SetDecoder(
      std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
          TextResourceDecoderOptions::kHTMLContent, Utf8Encoding())));
}

void ExternalPatchLoader::DataReceived(Resource* resource,
                                       base::span<const char> data) {
  CHECK(RuntimeEnabledFeatures::DeclarativeFragmentEnabled());
  if (ready_state_ != kErrorOccurred && parser_) {
    parser_->AppendBytes(base::as_bytes(data));
  }
}

void ExternalPatchLoader::NotifyFinished(Resource* resource) {
  CHECK(RuntimeEnabledFeatures::DeclarativeFragmentEnabled());
  if (parser_) {
    static_cast<DocumentParser*>(parser_.Get())->Finish();
    parser_ = nullptr;
  }

  const bool errored = ready_state_ == kErrorOccurred ||
                       resource->ErrorOccurred() ||
                       !resource->PassedIntegrityChecks();
  ready_state_ = errored ? kErrorOccurred : kReady;
  resource_ = nullptr;

  if (!errored) {
    // FIXME(crbug.com/545823942): handle error conditions
    owner_->Finalize(template_);
  }
}

void ExternalPatchLoader::Trace(Visitor* visitor) const {
  visitor->Trace(owner_);
  visitor->Trace(template_);
  visitor->Trace(resource_);
  visitor->Trace(parser_);
  RawResourceClient::Trace(visitor);
}

}  // namespace blink
