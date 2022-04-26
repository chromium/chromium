// Copyright 2016 The Chromium Authors. All rights reserved.
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
#include "third_party/blink/renderer/platform/wtf/date_math.h"

namespace blink {

namespace {

// TODO(crbug/1264960): Cleanup or remove this class.
class DeprecationInfo final {
 public:
  static const DeprecationInfo WithTranslation(
      WebFeature feature,
      const DeprecationIssueType& type) {
    return DeprecationInfo(feature, type, String(), String());
  }

  static const DeprecationInfo NotDeprecated(WebFeature feature) {
    return DeprecationInfo(feature, DeprecationIssueType::kUntranslated,
                           "NotDeprecated", String());
  }

  const WebFeature feature_;
  const DeprecationIssueType type_;
  const String id_;
  const String message_;

 private:
  DeprecationInfo(WebFeature feature,
                  DeprecationIssueType type,
                  const String& id,
                  const String& message)
      : feature_(feature), type_(type), id_(id), message_(message) {}
};

// TODO(crbug/1264960): Consider migrating this switch statement to
// third_party/blink/renderer/core/inspector/inspector_audits_issue.h to stop
// passing around protocol::Audits::DeprecationIssueType once all deprecations
// are translated. Alternatively, alphabetize this list.
const DeprecationInfo GetDeprecationInfo(WebFeature feature) {
  switch (feature) {
    // Quota
    case WebFeature::kPrefixedStorageInfo:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kPrefixedStorageInfo);

    case WebFeature::kPrefixedVideoSupportsFullscreen:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kPrefixedVideoSupportsFullscreen);

    case WebFeature::kPrefixedVideoDisplayingFullscreen:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kPrefixedVideoDisplayingFullscreen);

    case WebFeature::kPrefixedVideoEnterFullscreen:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kPrefixedVideoEnterFullscreen);

    case WebFeature::kPrefixedVideoExitFullscreen:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kPrefixedVideoExitFullscreen);

    case WebFeature::kPrefixedVideoEnterFullScreen:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kPrefixedVideoEnterFullScreen);

    case WebFeature::kPrefixedVideoExitFullScreen:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kPrefixedVideoExitFullScreen);

    case WebFeature::kPrefixedRequestAnimationFrame:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kPrefixedRequestAnimationFrame);

    case WebFeature::kPrefixedCancelAnimationFrame:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kPrefixedCancelAnimationFrame);

    case WebFeature::kPictureSourceSrc:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kPictureSourceSrc);

    case WebFeature::kXMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload:
      return DeprecationInfo::WithTranslation(
          feature,
          DeprecationIssueType::
              kXMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload);

    case WebFeature::kRangeExpand:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kRangeExpand);

    // Blocked subresource requests:
    case WebFeature::kRequestedSubresourceWithEmbeddedCredentials:
      return DeprecationInfo::WithTranslation(
          feature,
          DeprecationIssueType::kRequestedSubresourceWithEmbeddedCredentials);

    // Powerful features on insecure origins (https://goo.gl/rStTGz)
    case WebFeature::kGeolocationInsecureOrigin:
    case WebFeature::kGeolocationInsecureOriginIframe:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kGeolocationInsecureOrigin);

    case WebFeature::kGeolocationInsecureOriginDeprecatedNotRemoved:
    case WebFeature::kGeolocationInsecureOriginIframeDeprecatedNotRemoved:
      return DeprecationInfo::WithTranslation(
          feature,
          DeprecationIssueType::kGeolocationInsecureOriginDeprecatedNotRemoved);

    case WebFeature::kGetUserMediaInsecureOrigin:
    case WebFeature::kGetUserMediaInsecureOriginIframe:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kGetUserMediaInsecureOrigin);

    case WebFeature::kMediaSourceAbortRemove:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kMediaSourceAbortRemove);

    case WebFeature::kMediaSourceDurationTruncatingBuffered:
      return DeprecationInfo::WithTranslation(
          feature,
          DeprecationIssueType::kMediaSourceDurationTruncatingBuffered);

    case WebFeature::kNotificationInsecureOrigin:
    case WebFeature::kNotificationAPIInsecureOriginIframe:
    case WebFeature::kNotificationPermissionRequestedInsecureOrigin:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kNotificationInsecureOrigin);

    case WebFeature::kNotificationPermissionRequestedIframe:
      return DeprecationInfo::WithTranslation(
          feature,
          DeprecationIssueType::kNotificationPermissionRequestedIframe);

    case WebFeature::kBatteryStatusInsecureOrigin:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kBatteryStatusInsecureOrigin);

    case WebFeature::kCSSSelectorInternalMediaControlsOverlayCastButton:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::
                       kCSSSelectorInternalMediaControlsOverlayCastButton);

    case WebFeature::kSelectionAddRangeIntersect:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kSelectionAddRangeIntersect);

    case WebFeature::kRtcpMuxPolicyNegotiate:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kRtcpMuxPolicyNegotiate);

    case WebFeature::kCanRequestURLHTTPContainingNewline:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kCanRequestURLHTTPContainingNewline);

    case WebFeature::kLocalCSSFileExtensionRejected:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kLocalCSSFileExtensionRejected);

    case WebFeature::kChromeLoadTimesRequestTime:
    case WebFeature::kChromeLoadTimesStartLoadTime:
    case WebFeature::kChromeLoadTimesCommitLoadTime:
    case WebFeature::kChromeLoadTimesFinishDocumentLoadTime:
    case WebFeature::kChromeLoadTimesFinishLoadTime:
    case WebFeature::kChromeLoadTimesNavigationType:
    case WebFeature::kChromeLoadTimesConnectionInfo:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kChromeLoadTimesConnectionInfo);

    case WebFeature::kChromeLoadTimesFirstPaintTime:
    case WebFeature::kChromeLoadTimesFirstPaintAfterLoadTime:
      return DeprecationInfo::WithTranslation(
          feature,
          DeprecationIssueType::kChromeLoadTimesFirstPaintAfterLoadTime);

    case WebFeature::kChromeLoadTimesWasFetchedViaSpdy:
    case WebFeature::kChromeLoadTimesWasNpnNegotiated:
    case WebFeature::kChromeLoadTimesNpnNegotiatedProtocol:
    case WebFeature::kChromeLoadTimesWasAlternateProtocolAvailable:
      return DeprecationInfo::WithTranslation(
          feature,
          DeprecationIssueType::kChromeLoadTimesWasAlternateProtocolAvailable);

    case WebFeature::kMediaElementSourceOnOfflineContext:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kMediaElementAudioSourceNode);

    case WebFeature::kTextToSpeech_SpeakDisallowedByAutoplay:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kTextToSpeech_DisallowedByAutoplay);

    case WebFeature::kRTCPeerConnectionComplexPlanBSdpUsingDefaultSdpSemantics:
      return DeprecationInfo::WithTranslation(
          feature,
          DeprecationIssueType::
              kRTCPeerConnectionComplexPlanBSdpUsingDefaultSdpSemantics);

    case WebFeature::kNoSysexWebMIDIWithoutPermission:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kNoSysexWebMIDIWithoutPermission);

    case WebFeature::kCustomCursorIntersectsViewport:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kCustomCursorIntersectsViewport);

    case WebFeature::kXRSupportsSession:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kXRSupportsSession);

    case WebFeature::kObsoleteWebrtcTlsVersion:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kObsoleteWebRtcCipherSuite);

    case WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation:
      return DeprecationInfo::WithTranslation(
          feature,
          DeprecationIssueType::kSharedArrayBufferConstructedWithoutIsolation);

    case WebFeature::kRTCConstraintEnableRtpDataChannelsFalse:
    case WebFeature::kRTCConstraintEnableRtpDataChannelsTrue:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kRTPDataChannel);

    case WebFeature::kRTCPeerConnectionSdpSemanticsPlanB:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kRTCPeerConnectionSdpSemanticsPlanB);

    case WebFeature::kAddressSpacePublicNonSecureContextEmbeddedPrivate:
    case WebFeature::kAddressSpacePublicNonSecureContextEmbeddedLocal:
    case WebFeature::kAddressSpacePrivateNonSecureContextEmbeddedLocal:
      return DeprecationInfo::WithTranslation(
          feature,
          DeprecationIssueType::kInsecurePrivateNetworkSubresourceRequest);
    case WebFeature::kXHRJSONEncodingDetection:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kXHRJSONEncodingDetection);

    case WebFeature::kAuthorizationCoveredByWildcard:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kAuthorizationCoveredByWildcard);

    case WebFeature::kRTCConstraintEnableDtlsSrtpTrue:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kRTCConstraintEnableDtlsSrtpTrue);

    case WebFeature::kRTCConstraintEnableDtlsSrtpFalse:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kRTCConstraintEnableDtlsSrtpFalse);
    case WebFeature::kV8SharedArrayBufferConstructedInExtensionWithoutIsolation:
      return DeprecationInfo::WithTranslation(
          feature,
          DeprecationIssueType::
              kV8SharedArrayBufferConstructedInExtensionWithoutIsolation);

    case WebFeature::kCrossOriginWindowAlert:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kCrossOriginWindowAlert);
    case WebFeature::kCrossOriginWindowConfirm:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kCrossOriginWindowConfirm);

    case WebFeature::kPaymentRequestBasicCard:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kPaymentRequestBasicCard);

    case WebFeature::kPaymentRequestShowWithoutGesture:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kPaymentRequestShowWithoutGesture);

    case WebFeature::kHostCandidateAttributeGetter:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kHostCandidateAttributeGetter);

    case WebFeature::kWebCodecsVideoFrameDefaultTimestamp:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kWebCodecsVideoFrameDefaultTimestamp);

    case WebFeature::kDocumentDomainSettingWithoutOriginAgentClusterHeader:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::
                       kDocumentDomainSettingWithoutOriginAgentClusterHeader);

    case WebFeature::kCrossOriginAccessBasedOnDocumentDomain:
      return DeprecationInfo::WithTranslation(
          feature,
          DeprecationIssueType::kCrossOriginAccessBasedOnDocumentDomain);

    case WebFeature::kCookieWithTruncatingChar:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kCookieWithTruncatingChar);

    case WebFeature::kEventPath:
      return DeprecationInfo::WithTranslation(feature,
                                              DeprecationIssueType::kEventPath);

    case WebFeature::kDeprecationExample:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kDeprecationExample);

    case WebFeature::kRTCPeerConnectionLegacyCreateWithMediaConstraints:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::
                       kRTCPeerConnectionLegacyCreateWithMediaConstraints);

    case WebFeature::kLegacyConstraintGoogScreencastMinBitrate:
      return DeprecationInfo::WithTranslation(
          feature,
          DeprecationIssueType::kLegacyConstraintGoogScreencastMinBitrate);

    case WebFeature::kLegacyConstraintGoogIPv6:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kLegacyConstraintGoogIPv6);

    case WebFeature::kLegacyConstraintGoogSuspendBelowMinBitrate:
      return DeprecationInfo::WithTranslation(
          feature,
          DeprecationIssueType::kLegacyConstraintGoogSuspendBelowMinBitrate);

    case WebFeature::kLegacyConstraintGoogCpuOveruseDetection:
      return DeprecationInfo::WithTranslation(
          feature,
          DeprecationIssueType::kLegacyConstraintGoogCpuOveruseDetection);

    // Features that aren't deprecated don't have a deprecation message.
    default:
      return DeprecationInfo::NotDeprecated(feature);
  }
}

Report* CreateReportInternal(const KURL& context_url,
                             const DeprecationInfo& info) {
  DeprecationReportBody* body;
  if (info.type_ == DeprecationIssueType::kUntranslated) {
    body = MakeGarbageCollected<DeprecationReportBody>(info.id_, absl::nullopt,
                                                       info.message_);
  } else {
    body = MakeGarbageCollected<DeprecationReportBody>(
        String::Number(static_cast<int>(info.feature_)), absl::nullopt,
        "Deprecation messages are stored in the devtools-frontend repo at "
        "front_end/models/issues_manager/DeprecationIssue.ts.");
  }
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
  AuditsIssue::ReportDeprecationIssue(context, info.type_, info.message_,
                                      info.id_);

  Report* report = CreateReportInternal(context->Url(), info);

  // Send the deprecation report to the Reporting API and any
  // ReportingObservers.
  ReportingContext::From(context)->QueueReport(report);
}

// static
String Deprecation::DeprecationMessage(WebFeature feature) {
  return GetDeprecationInfo(feature).message_;
}

}  // namespace blink
