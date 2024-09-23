// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/frame_load_request.h"

#include "base/types/optional_util.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace {

static void SetReferrerForRequest(LocalDOMWindow* origin_window,
                                  ResourceRequest& request) {
  DCHECK(origin_window);

  // Always use the initiating window to generate the referrer. We need to
  // generateReferrer(), because we haven't enforced
  // network::mojom::ReferrerPolicy or https->http referrer suppression yet.
  String referrer_to_use = request.ReferrerString();
  network::mojom::ReferrerPolicy referrer_policy_to_use =
      request.GetReferrerPolicy();

  if (referrer_to_use == Referrer::ClientReferrerString())
    referrer_to_use = origin_window->OutgoingReferrer();

  if (referrer_policy_to_use == network::mojom::ReferrerPolicy::kDefault)
    referrer_policy_to_use = origin_window->GetReferrerPolicy();

  Referrer referrer = SecurityPolicy::GenerateReferrer(
      referrer_policy_to_use, request.Url(), referrer_to_use);

  request.SetReferrerString(referrer.referrer);
  request.SetReferrerPolicy(referrer.referrer_policy);
  request.SetHTTPOriginToMatchReferrerIfNeeded();
}

void LogDanglingMarkupHistogram(LocalDOMWindow* origin_window,
                                const AtomicString& target) {
  DCHECK(origin_window);

  origin_window->CountUse(WebFeature::kDanglingMarkupInTarget);
  if (!target.EndsWith('>')) {
    origin_window->CountUse(WebFeature::kDanglingMarkupInTargetNotEndsWithGT);
    if (!target.EndsWith('\n')) {
      origin_window->CountUse(
          WebFeature::kDanglingMarkupInTargetNotEndsWithNewLineOrGT);
    }
  }
}

bool ContainsNewLineAndLessThan(const AtomicString& target) {
  return (target.Contains('\n') || target.Contains('\r') ||
          target.Contains('\t')) &&
         target.Contains('<');
}

}  // namespace

FrameLoadRequest::FrameLoadRequest(LocalDOMWindow* origin_window,
                                   const ResourceRequest& resource_request)
    : origin_window_(origin_window), should_send_referrer_(kMaybeSendReferrer) {
  resource_request_.CopyHeadFrom(resource_request);
  resource_request_.SetHttpBody(resource_request.HttpBody());
  resource_request_.SetMode(network::mojom::RequestMode::kNavigate);
  resource_request_.SetTargetAddressSpace(
      network::mojom::IPAddressSpace::kUnknown);
  resource_request_.SetCredentialsMode(
      network::mojom::CredentialsMode::kInclude);
  resource_request_.SetRedirectMode(network::mojom::RedirectMode::kManual);

  if (const WebInputEvent* input_event = CurrentInputEvent::Get())
    SetInputStartTime(input_event->TimeStamp());

  if (origin_window) {
    world_ = origin_window->GetCurrentWorld();

    DCHECK(!resource_request_.RequestorOrigin());
    resource_request_.SetRequestorOrigin(origin_window->GetSecurityOrigin());
    // Note: `resource_request_` is owned by this FrameLoadRequest instance, and
    // its url doesn't change after this point, so it's ok to check for
    // about:blank and about:srcdoc here.
    if (resource_request_.Url().IsAboutBlankURL() ||
        resource_request_.Url().IsAboutSrcdocURL() ||
        resource_request_.Url().IsEmpty()) {
      requestor_base_url_ = origin_window->BaseURL();
    }

    if (resource_request.Url().ProtocolIs("blob")) {
      blob_url_token_ = base::MakeRefCounted<
          base::RefCountedData<mojo::Remote<mojom::blink::BlobURLToken>>>();
      origin_window->GetPublicURLManager().Resolve(
          resource_request.Url(),
          blob_url_token_->data.BindNewPipeAndPassReceiver());
    }

    SetReferrerForRequest(origin_window, resource_request_);

    SetSourceLocation(CaptureSourceLocation(origin_window));
  }
}

FrameLoadRequest::FrameLoadRequest(
    LocalDOMWindow* origin_window,
    const ResourceRequestHead& resource_request_head)
    : FrameLoadRequest(origin_window, ResourceRequest(resource_request_head)) {}

HTMLFormElement* FrameLoadRequest::Form() const {
  if (IsA<HTMLFormElement>(source_element_)) {
    return To<HTMLFormElement>(source_element_);
  }
  if (IsA<HTMLFormControlElement>(source_element_)) {
    return To<HTMLFormControlElement>(source_element_)->formOwner();
  }
  return nullptr;
}

bool FrameLoadRequest::CanDisplay(const KURL& url) const {
  DCHECK(!origin_window_ || origin_window_->GetSecurityOrigin() ==
                                resource_request_.RequestorOrigin());
  return resource_request_.CanDisplay(url);
}

const LocalFrameToken* FrameLoadRequest::GetInitiatorFrameToken() const {
  return base::OptionalToPtr(initiator_frame_token_);
}

const AtomicString& FrameLoadRequest::CleanNavigationTarget(
    const AtomicString& target) const {
  if (ContainsNewLineAndLessThan(target)) {
    LogDanglingMarkupHistogram(origin_window_, target);
    if (RuntimeEnabledFeatures::RemoveDanglingMarkupInTargetEnabled()) {
      DEFINE_STATIC_LOCAL(const AtomicString, blank, ("_blank"));
      return blank;
    }
  }
  return target;
}

}  // namespace blink
