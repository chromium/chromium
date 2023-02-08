// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/reporting/reporting.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation_report_body.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

const char kNotDeprecated[] = "NotDeprecated";

class DeprecationInfo final {
 public:
  static const DeprecationInfo WithTranslation(WebFeature feature,
                                               const String& type) {
    return DeprecationInfo(feature, type);
  }

  static const DeprecationInfo NotDeprecated(WebFeature feature) {
    return DeprecationInfo(feature, kNotDeprecated);
  }

  const WebFeature feature_;
  const String type_;

 private:
  DeprecationInfo(WebFeature feature, String type)
      : feature_(feature), type_(type) {}
};

const DeprecationInfo GetDeprecationInfo(WebFeature feature) {
  // Please keep this alphabetized by DeprecationIssueType.
  switch (feature) {
    case WebFeature::kAuthorizationCoveredByWildcard:
      return DeprecationInfo::WithTranslation(feature,
                                              "AuthorizationCoveredByWildcard");
    case WebFeature::kCanRequestURLHTTPContainingNewline:
      return DeprecationInfo::WithTranslation(
          feature, "CanRequestURLHTTPContainingNewline");
    case WebFeature::kChromeLoadTimesCommitLoadTime:
    case WebFeature::kChromeLoadTimesConnectionInfo:
    case WebFeature::kChromeLoadTimesFinishDocumentLoadTime:
    case WebFeature::kChromeLoadTimesFinishLoadTime:
    case WebFeature::kChromeLoadTimesNavigationType:
    case WebFeature::kChromeLoadTimesRequestTime:
    case WebFeature::kChromeLoadTimesStartLoadTime:
      return DeprecationInfo::WithTranslation(feature,
                                              "ChromeLoadTimesConnectionInfo");
    case WebFeature::kChromeLoadTimesFirstPaintAfterLoadTime:
    case WebFeature::kChromeLoadTimesFirstPaintTime:
      return DeprecationInfo::WithTranslation(
          feature, "ChromeLoadTimesFirstPaintAfterLoadTime");
    case WebFeature::kChromeLoadTimesWasAlternateProtocolAvailable:
    case WebFeature::kChromeLoadTimesWasFetchedViaSpdy:
    case WebFeature::kChromeLoadTimesNpnNegotiatedProtocol:
    case WebFeature::kChromeLoadTimesWasNpnNegotiated:
      return DeprecationInfo::WithTranslation(
          feature, "ChromeLoadTimesWasAlternateProtocolAvailable");
    case WebFeature::kCookieWithTruncatingChar:
      return DeprecationInfo::WithTranslation(feature,
                                              "CookieWithTruncatingChar");
    case WebFeature::kCrossOriginAccessBasedOnDocumentDomain:
      return DeprecationInfo::WithTranslation(
          feature, "CrossOriginAccessBasedOnDocumentDomain");
    case WebFeature::kCrossOriginWindowAlert:
      return DeprecationInfo::WithTranslation(feature,
                                              "CrossOriginWindowAlert");
    case WebFeature::kCrossOriginWindowConfirm:
      return DeprecationInfo::WithTranslation(feature,
                                              "CrossOriginWindowConfirm");
    case WebFeature::kCSSSelectorInternalMediaControlsOverlayCastButton:
      return DeprecationInfo::WithTranslation(
          feature, "CSSSelectorInternalMediaControlsOverlayCastButton");
    case WebFeature::kDeprecationExample:
      return DeprecationInfo::WithTranslation(feature, "DeprecationExample");
    case WebFeature::kDocumentDomainSettingWithoutOriginAgentClusterHeader:
      return DeprecationInfo::WithTranslation(
          feature, "DocumentDomainSettingWithoutOriginAgentClusterHeader");
    case WebFeature::kEventPath:
      return DeprecationInfo::WithTranslation(feature, "EventPath");
    case WebFeature::kExpectCTHeader:
      return DeprecationInfo::WithTranslation(feature, "ExpectCTHeader");
    // Powerful features on insecure origins (https://goo.gl/rStTGz)
    case WebFeature::kGeolocationInsecureOrigin:
    case WebFeature::kGeolocationInsecureOriginIframe:
      return DeprecationInfo::WithTranslation(feature,
                                              "GeolocationInsecureOrigin");
    case WebFeature::kGeolocationInsecureOriginDeprecatedNotRemoved:
    case WebFeature::kGeolocationInsecureOriginIframeDeprecatedNotRemoved:
      return DeprecationInfo::WithTranslation(
          feature, "GeolocationInsecureOriginDeprecatedNotRemoved");
    case WebFeature::kGetUserMediaInsecureOrigin:
    case WebFeature::kGetUserMediaInsecureOriginIframe:
      return DeprecationInfo::WithTranslation(feature,
                                              "GetUserMediaInsecureOrigin");
    case WebFeature::kHostCandidateAttributeGetter:
      return DeprecationInfo::WithTranslation(feature,
                                              "HostCandidateAttributeGetter");
    case WebFeature::kAddressSpacePrivateNonSecureContextEmbeddedLocal:
    case WebFeature::kAddressSpacePublicNonSecureContextEmbeddedLocal:
    case WebFeature::kAddressSpacePublicNonSecureContextEmbeddedPrivate:
      return DeprecationInfo::WithTranslation(
          feature, "InsecurePrivateNetworkSubresourceRequest");
    case WebFeature::kLocalCSSFileExtensionRejected:
      return DeprecationInfo::WithTranslation(feature,
                                              "LocalCSSFileExtensionRejected");
    case WebFeature::kMediaSourceAbortRemove:
      return DeprecationInfo::WithTranslation(feature,
                                              "MediaSourceAbortRemove");
    case WebFeature::kMediaSourceDurationTruncatingBuffered:
      return DeprecationInfo::WithTranslation(
          feature, "MediaSourceDurationTruncatingBuffered");
    case WebFeature::kNoSysexWebMIDIWithoutPermission:
      return DeprecationInfo::WithTranslation(
          feature, "NoSysexWebMIDIWithoutPermission");
    case WebFeature::kNotificationAPIInsecureOriginIframe:
    case WebFeature::kNotificationInsecureOrigin:
    case WebFeature::kNotificationPermissionRequestedInsecureOrigin:
      return DeprecationInfo::WithTranslation(feature,
                                              "NotificationInsecureOrigin");
    case WebFeature::kNotificationPermissionRequestedIframe:
      return DeprecationInfo::WithTranslation(
          feature, "NotificationPermissionRequestedIframe");
    case WebFeature::kObsoleteWebrtcTlsVersion:
      return DeprecationInfo::WithTranslation(feature,
                                              "ObsoleteWebRtcCipherSuite");
    case WebFeature::kObsoleteCreateImageBitmapImageOrientationNone:
      return DeprecationInfo::WithTranslation(
          feature, "ObsoleteCreateImageBitmapImageOrientationNone");
    case WebFeature::kPaymentInstruments:
      return DeprecationInfo::WithTranslation(feature, "PaymentInstruments");
    case WebFeature::kPaymentRequestCSPViolation:
      return DeprecationInfo::WithTranslation(feature,
                                              "PaymentRequestCSPViolation");
    case WebFeature::kPersistentQuotaType:
      return DeprecationInfo::WithTranslation(feature, "PersistentQuotaType");
    case WebFeature::kPictureSourceSrc:
      return DeprecationInfo::WithTranslation(feature, "PictureSourceSrc");
    case WebFeature::kPrefixedCancelAnimationFrame:
      return DeprecationInfo::WithTranslation(feature,
                                              "PrefixedCancelAnimationFrame");
    case WebFeature::kPrefixedRequestAnimationFrame:
      return DeprecationInfo::WithTranslation(feature,
                                              "PrefixedRequestAnimationFrame");
    // Quota
    case WebFeature::kPrefixedStorageInfo:
      return DeprecationInfo::WithTranslation(feature, "PrefixedStorageInfo");
    case WebFeature::kPrefixedVideoDisplayingFullscreen:
      return DeprecationInfo::WithTranslation(
          feature, "PrefixedVideoDisplayingFullscreen");
    case WebFeature::kPrefixedVideoEnterFullScreen:
      return DeprecationInfo::WithTranslation(feature,
                                              "PrefixedVideoEnterFullScreen");
    case WebFeature::kPrefixedVideoEnterFullscreen:
      return DeprecationInfo::WithTranslation(feature,
                                              "PrefixedVideoEnterFullscreen");
    case WebFeature::kPrefixedVideoExitFullScreen:
      return DeprecationInfo::WithTranslation(feature,
                                              "PrefixedVideoExitFullScreen");
    case WebFeature::kPrefixedVideoExitFullscreen:
      return DeprecationInfo::WithTranslation(feature,
                                              "PrefixedVideoExitFullscreen");
    case WebFeature::kPrefixedVideoSupportsFullscreen:
      return DeprecationInfo::WithTranslation(
          feature, "PrefixedVideoSupportsFullscreen");
    case WebFeature::kRangeExpand:
      return DeprecationInfo::WithTranslation(feature, "RangeExpand");
    // Blocked subresource requests:
    case WebFeature::kRequestedSubresourceWithEmbeddedCredentials:
      return DeprecationInfo::WithTranslation(
          feature, "RequestedSubresourceWithEmbeddedCredentials");
    case WebFeature::kRTCConstraintEnableDtlsSrtpFalse:
      return DeprecationInfo::WithTranslation(
          feature, "RTCConstraintEnableDtlsSrtpFalse");
    case WebFeature::kRTCConstraintEnableDtlsSrtpTrue:
      return DeprecationInfo::WithTranslation(
          feature, "RTCConstraintEnableDtlsSrtpTrue");
    case WebFeature::kRtcpMuxPolicyNegotiate:
      return DeprecationInfo::WithTranslation(feature,
                                              "RtcpMuxPolicyNegotiate");
    case WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation:
      return DeprecationInfo::WithTranslation(
          feature, "SharedArrayBufferConstructedWithoutIsolation");
    case WebFeature::kTextToSpeech_SpeakDisallowedByAutoplay:
      return DeprecationInfo::WithTranslation(
          feature, "TextToSpeech_DisallowedByAutoplay");
    case WebFeature::kV8SharedArrayBufferConstructedInExtensionWithoutIsolation:
      return DeprecationInfo::WithTranslation(
          feature, "V8SharedArrayBufferConstructedInExtensionWithoutIsolation");
    case WebFeature::kXHRJSONEncodingDetection:
      return DeprecationInfo::WithTranslation(feature,
                                              "XHRJSONEncodingDetection");
    case WebFeature::kXMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload:
      return DeprecationInfo::WithTranslation(
          feature, "XMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload");
    case WebFeature::kXRSupportsSession:
      return DeprecationInfo::WithTranslation(feature, "XRSupportsSession");
    case WebFeature::kIdentityInCanMakePaymentEvent:
      return DeprecationInfo::WithTranslation(feature,
                                              "IdentityInCanMakePaymentEvent");
    case WebFeature::kExplicitOverflowVisibleOnReplacedElement:
      return DeprecationInfo::WithTranslation(
          feature, "OverflowVisibleOnReplacedElement");
    default:
      return DeprecationInfo::NotDeprecated(feature);
  }
}

Report* CreateReportInternal(const KURL& context_url,
                             const DeprecationInfo& info) {
  DeprecationReportBody* body = MakeGarbageCollected<DeprecationReportBody>(
      String::Number(static_cast<int>(info.feature_)), absl::nullopt,
      "Deprecation messages are stored in the devtools-frontend repo at "
      "front_end/models/issues_manager/DeprecationIssue.ts.");
  return MakeGarbageCollected<Report>(ReportType::kDeprecation, context_url,
                                      body);
}

}  // anonymous namespace

Deprecation::Deprecation() : mute_count_(0) {}

void Deprecation::ClearSuppression() {
  features_deprecation_bits_.reset();
}

void Deprecation::MuteForInspector() {
  mute_count_++;
}

void Deprecation::UnmuteForInspector() {
  mute_count_--;
}

void Deprecation::SetReported(WebFeature feature) {
  features_deprecation_bits_.set(static_cast<size_t>(feature));
}

bool Deprecation::GetReported(WebFeature feature) const {
  return features_deprecation_bits_[static_cast<size_t>(feature)];
}

void Deprecation::CountDeprecationCrossOriginIframe(LocalDOMWindow* window,
                                                    WebFeature feature) {
  DCHECK(window);
  if (!window->GetFrame())
    return;

  // Check to see if the frame can script into the top level context.
  Frame& top = window->GetFrame()->Tree().Top();
  if (!window->GetSecurityOrigin()->CanAccess(
          top.GetSecurityContext()->GetSecurityOrigin())) {
    CountDeprecation(window, feature);
  }
}

void Deprecation::CountDeprecation(ExecutionContext* context,
                                   WebFeature feature) {
  if (!context)
    return;

  Deprecation* deprecation = nullptr;
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    if (window->GetFrame())
      deprecation = &window->GetFrame()->GetPage()->GetDeprecation();
  } else if (auto* scope = DynamicTo<WorkerOrWorkletGlobalScope>(context)) {
    // TODO(crbug.com/1146824): Remove this once PlzDedicatedWorker and
    // PlzServiceWorker ship.
    if (!scope->IsInitialized()) {
      return;
    }
    deprecation = &scope->GetDeprecation();
  }

  if (!deprecation || deprecation->mute_count_ ||
      deprecation->GetReported(feature)) {
    return;
  }
  deprecation->SetReported(feature);
  context->CountUse(feature);
  const DeprecationInfo info = GetDeprecationInfo(feature);

  // Send the deprecation message as a DevTools issue.
  AuditsIssue::ReportDeprecationIssue(context, info.type_);

  Report* report = CreateReportInternal(context->Url(), info);

  // Send the deprecation report to the Reporting API and any
  // ReportingObservers.
  ReportingContext::From(context)->QueueReport(report);
}

// static
bool Deprecation::IsDeprecated(WebFeature feature) {
  return GetDeprecationInfo(feature).type_ != kNotDeprecated;
}

}  // namespace blink
