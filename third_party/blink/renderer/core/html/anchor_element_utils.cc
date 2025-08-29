// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_utils.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/navigation_policy.h"
#include "third_party/blink/renderer/core/navigation_api/navigate_event_dispatch_params.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// The download attribute specifies a filename, and an excessively long one can
// crash the browser process. Filepaths probably can't be longer than 4096
// characters, but this is enough to prevent the browser process from becoming
// unresponsive or crashing.
inline constexpr int kMaxDownloadAttrLength = 1000000;

// Note: Here it covers download originated from clicking on <a download> link
// that results in direct download. Features in this method can also be logged
// from browser for download due to navigations to non-web-renderable content.
bool ShouldInterveneDownloadByFramePolicy(LocalFrame* frame) {
  bool should_intervene_download = false;
  Document& document = *(frame->GetDocument());
  UseCounter::Count(document, WebFeature::kDownloadPrePolicyCheck);
  const bool has_gesture = LocalFrame::HasTransientUserActivation(frame);
  if (!has_gesture) {
    UseCounter::Count(document, WebFeature::kDownloadWithoutUserGesture);
  }
  if (frame->IsAdFrame()) {
    UseCounter::Count(document, WebFeature::kDownloadInAdFrame);
    if (!has_gesture) {
      UseCounter::Count(document,
                        WebFeature::kDownloadInAdFrameWithoutUserGesture);
      should_intervene_download = true;
    }
  }
  if (frame->DomWindow()->IsSandboxed(
          network::mojom::blink::WebSandboxFlags::kDownloads)) {
    UseCounter::Count(document, WebFeature::kDownloadInSandbox);
    should_intervene_download = true;
  }
  if (!should_intervene_download) {
    UseCounter::Count(document, WebFeature::kDownloadPostPolicyCheck);
  }
  return should_intervene_download;
}

bool ValidateDownloadAttributeLength(const String& download_attr,
                                     Element* element) {
  if (download_attr.length() > kMaxDownloadAttrLength) {
    element->AddConsoleMessage(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kError,
        String::Format("Download attribute for anchor element is too long. "
                       "Max: %d, given: %d",
                       kMaxDownloadAttrLength, download_attr.length()));
    return false;
  }
  return true;
}

}  // namespace

void AnchorElementUtils::HandleDownloadAttribute(Element* element,
                                                 const String& download_attr,
                                                 const KURL& url,
                                                 LocalDOMWindow* window,
                                                 bool is_trusted,
                                                 ResourceRequest request) {
  LocalFrame* frame = window->GetFrame();
  if (ShouldInterveneDownloadByFramePolicy(frame)) {
    return;
  }

  if (!ValidateDownloadAttributeLength(download_attr, element)) {
    return;
  }

  auto* params = MakeGarbageCollected<NavigateEventDispatchParams>(
      url, NavigateEventType::kCrossDocument, WebFrameLoadType::kStandard);
  if (is_trusted) {
    params->involvement = UserNavigationInvolvement::kActivation;
  }
  params->download_filename = download_attr;
  params->source_element = element;

  if (window->navigation()->DispatchNavigateEvent(params) !=
      NavigationApi::DispatchResult::kContinue) {
    return;
  }

  // A download will never notify blink about its completion. Tell the
  // NavigationApi that the navigation was dropped, so that it doesn't
  // leave the frame thinking it is loading indefinitely.
  window->navigation()->InformAboutCanceledNavigation(
      CancelNavigationReason::kDropped);

  request.SetSuggestedFilename(download_attr);
  request.SetRequestContext(mojom::blink::RequestContextType::DOWNLOAD);
  request.SetRequestorOrigin(window->GetSecurityOrigin());
  network::mojom::ReferrerPolicy referrer_policy = request.GetReferrerPolicy();

  if (referrer_policy == network::mojom::ReferrerPolicy::kDefault) {
    referrer_policy = window->GetReferrerPolicy();
  }

  Referrer referrer = SecurityPolicy::GenerateReferrer(
      referrer_policy, url, window->OutgoingReferrer());
  request.SetReferrerString(referrer.referrer);
  request.SetReferrerPolicy(referrer.referrer_policy);
  frame->DownloadURL(request, network::mojom::blink::RedirectMode::kManual);

  return;
}

}  // namespace blink
