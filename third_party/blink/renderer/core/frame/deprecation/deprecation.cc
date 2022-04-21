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

enum Milestone {
  kUnknown,
  kM61 = 61,
  kM62 = 62,
  kM64 = 64,
  kM65 = 65,
  kM70 = 70,
  kM71 = 71,
  kM72 = 72,
  kM75 = 75,
  kM76 = 76,
  kM77 = 77,
  kM78 = 78,
  kM79 = 79,
  kM80 = 80,
  kM81 = 81,
  kM82 = 82,
  kM83 = 83,
  kM84 = 84,
  kM85 = 85,
  kM86 = 86,
  kM87 = 87,
  kM88 = 88,
  kM89 = 89,
  kM90 = 90,
  kM91 = 91,
  kM92 = 92,
  kM93 = 93,
  kM94 = 94,
  kM95 = 95,
  kM96 = 96,
  kM97 = 97,
  kM98 = 98,
  kM99 = 99,
  kM100 = 100,
  kM101 = 101,
  kM102 = 102,
  kM103 = 103,
  kM104 = 104,
  kM105 = 105,
  kM106 = 106,
  kM107 = 107,
  kM108 = 108,
  kM109 = 109,
};

// Returns estimated milestone dates as milliseconds since January 1, 1970.
base::Time::Exploded MilestoneDate(Milestone milestone) {
  // These are the Estimated Stable Dates:
  // https://chromiumdash.appspot.com/schedule
  // All dates except for kUnknown are at 04:00:00 GMT.
  switch (milestone) {
    case kUnknown:
      break;
    case kM61:
      return {2017, 9, 0, 5, 4};
    case kM62:
      return {2017, 10, 0, 17, 4};
    case kM64:
      return {2018, 1, 0, 23, 4};
    case kM65:
      return {2018, 3, 0, 6, 4};
    case kM70:
      return {2018, 10, 0, 16, 4};
    case kM71:
      return {2018, 12, 0, 4, 4};
    case kM72:
      return {2019, 1, 0, 29, 4};
    case kM75:
      return {2019, 6, 0, 4, 4};
    case kM76:
      return {2019, 7, 0, 30, 4};
    case kM77:
      return {2019, 9, 0, 10, 4};
    case kM78:
      return {2019, 10, 0, 22, 4};
    case kM79:
      return {2019, 12, 0, 10, 4};
    case kM80:
      return {2020, 2, 0, 4, 4};
    case kM81:
      return {2020, 4, 0, 7, 4};
    case kM82:
      // This release was cancelled, so this is the (new) M83 date.
      // https://groups.google.com/a/chromium.org/d/msg/chromium-dev/N1NxbSVOZas/ySlEKDKkBgAJ
      return {2020, 5, 0, 18, 4};
    case kM83:
      return {2020, 5, 0, 18, 4};
    case kM84:
      return {2020, 7, 0, 14, 4};
    case kM85:
      return {2020, 8, 0, 25, 4};
    case kM86:
      return {2020, 10, 0, 6, 4};
    case kM87:
      return {2020, 11, 0, 17, 4};
    case kM88:
      return {2021, 1, 0, 19, 4};
    case kM89:
      return {2021, 3, 0, 2, 4};
    case kM90:
      return {2021, 4, 0, 13, 4};
    case kM91:
      return {2021, 5, 0, 25, 4};
    case kM92:
      return {2021, 7, 0, 20, 4};
    case kM93:
      return {2021, 8, 0, 31, 4};
    case kM94:
      return {2021, 9, 0, 21, 4};
    case kM95:
      return {2021, 10, 0, 19, 4};
    case kM96:
      return {2022, 11, 0, 16, 4};
    case kM97:
      return {2022, 1, 0, 4, 4};
    case kM98:
      return {2022, 2, 0, 1, 4};
    case kM99:
      return {2022, 3, 0, 1, 4};
    case kM100:
      return {2022, 3, 0, 29, 4};
    case kM101:
      return {2022, 4, 0, 26, 4};
    case kM102:
      return {2022, 5, 0, 24, 4};
    case kM103:
      return {2022, 6, 0, 21, 4};
    case kM104:
      return {2022, 7, 0, 26, 4};
    case kM105:
      return {2022, 8, 0, 30, 4};
    case kM106:
      return {2022, 9, 0, 27, 4};
    case kM107:
      return {2022, 10, 0, 25, 4};
    case kM108:
      return {2022, 11, 0, 29, 4};
    case kM109:
      return {2023, 1, 0, 10, 4};
  }

  NOTREACHED();
  return {1970, 1, 0, 1, 0};
}

// Returns estimated milestone dates as human-readable strings.
String MilestoneString(Milestone milestone) {
  if (milestone == kUnknown)
    return String();
  base::Time::Exploded date = MilestoneDate(milestone);
  return String::Format("M%d, around %s %d", milestone,
                        WTF::kMonthFullName[date.month - 1], date.year);
}

class DeprecationInfo final {
 public:
  // Use this to inform developers of any `details` for the deprecation with
  // info at `chrome_status_id`. Use this format only if none of the ones below
  // make sense.
  static const DeprecationInfo WithDetailsAndChromeStatusID(
      WebFeature feature,
      const String& id,
      Milestone milestone,
      const String& details,
      const String& chrome_status_id) {
    return DeprecationInfo(
        feature, DeprecationIssueType::kUntranslated, id,
        String::Format(
            "%s See https://www.chromestatus.com/feature/%s for more details.",
            details.Ascii().c_str(), chrome_status_id.Ascii().c_str()));
  }

  // Use this to inform developers a deprecated `feature` has a `replacement`.
  static const DeprecationInfo WithFeatureAndReplacement(
      WebFeature feature,
      const String& id,
      Milestone milestone,
      const String& feature_name,
      const String& replacement) {
    return DeprecationInfo(
        feature, DeprecationIssueType::kUntranslated, id,
        String::Format("%s is deprecated. Please use %s instead.",
                       feature_name.Ascii().c_str(),
                       replacement.Ascii().c_str()));
  }

  // Use this to inform developers a deprecated `feature` has info at
  // `chrome_status_id`.
  static const DeprecationInfo WithFeatureAndChromeStatusID(
      WebFeature feature,
      const String& id,
      Milestone milestone,
      const String& feature_name,
      const String& chrome_status_id) {
    return DeprecationInfo(
        feature, DeprecationIssueType::kUntranslated, id,
        String::Format(
            "%s is deprecated and will be removed in %s. See "
            "https://www.chromestatus.com/feature/%s for more details.",
            feature_name.Ascii().c_str(),
            MilestoneString(milestone).Ascii().c_str(),
            chrome_status_id.Ascii().c_str()));
  }

  // Use this to inform developers a deprecated `feature` has a `replacement`
  // and info at `chrome_status_id`.
  static const DeprecationInfo WithFeatureAndReplacementAndChromeStatusID(
      WebFeature feature,
      const String& id,
      Milestone milestone,
      const String& feature_name,
      const String& replacement,
      const String& chrome_status_id) {
    return DeprecationInfo(
        feature, DeprecationIssueType::kUntranslated, id,
        String::Format(
            "%s is deprecated and will be removed in %s. Please use %s "
            "instead. See https://www.chromestatus.com/feature/%s for more "
            "details.",
            feature_name.Ascii().c_str(),
            MilestoneString(milestone).Ascii().c_str(),
            replacement.Ascii().c_str(), chrome_status_id.Ascii().c_str()));
  }

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
// are translated.
const DeprecationInfo GetDeprecationInfo(WebFeature feature) {
  switch (feature) {
    // Quota
    case WebFeature::kPrefixedStorageInfo:
      return DeprecationInfo::WithFeatureAndReplacement(
          feature, "PrefixedStorageInfo", kUnknown,
          "'window.webkitStorageInfo'",
          "'navigator.webkitTemporaryStorage' or "
          "'navigator.webkitPersistentStorage'");

    case WebFeature::kPrefixedVideoSupportsFullscreen:
      return DeprecationInfo::WithFeatureAndReplacement(
          feature, "PrefixedVideoSupportsFullscreen", kUnknown,
          "'HTMLVideoElement.webkitSupportsFullscreen'",
          "'Document.fullscreenEnabled'");

    case WebFeature::kPrefixedVideoDisplayingFullscreen:
      return DeprecationInfo::WithFeatureAndReplacement(
          feature, "PrefixedVideoDisplayingFullscreen", kUnknown,
          "'HTMLVideoElement.webkitDisplayingFullscreen'",
          "'Document.fullscreenElement'");

    case WebFeature::kPrefixedVideoEnterFullscreen:
      return DeprecationInfo::WithFeatureAndReplacement(
          feature, "PrefixedVideoEnterFullscreen", kUnknown,
          "'HTMLVideoElement.webkitEnterFullscreen()'",
          "'Element.requestFullscreen()'");

    case WebFeature::kPrefixedVideoExitFullscreen:
      return DeprecationInfo::WithFeatureAndReplacement(
          feature, "PrefixedVideoExitFullscreen", kUnknown,
          "'HTMLVideoElement.webkitExitFullscreen()'",
          "'Document.exitFullscreen()'");

    case WebFeature::kPrefixedVideoEnterFullScreen:
      return DeprecationInfo::WithFeatureAndReplacement(
          feature, "PrefixedVideoEnterFullScreen", kUnknown,
          "'HTMLVideoElement.webkitEnterFullScreen()'",
          "'Element.requestFullscreen()'");

    case WebFeature::kPrefixedVideoExitFullScreen:
      return DeprecationInfo::WithFeatureAndReplacement(
          feature, "PrefixedVideoExitFullScreen", kUnknown,
          "'HTMLVideoElement.webkitExitFullScreen()'",
          "'Document.exitFullscreen()'");

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
      return DeprecationInfo::WithFeatureAndReplacement(
          feature, "RangeExpand", kUnknown, "'Range.expand()'",
          "'Selection.modify()'");

    // Blocked subresource requests:
    case WebFeature::kRequestedSubresourceWithEmbeddedCredentials:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          feature, "RequestedSubresourceWithEmbeddedCredentials", kUnknown,
          "Subresource requests whose URLs contain embedded credentials (e.g. "
          "`https://user:pass@host/`) are blocked.",
          "5669008342777856");

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
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          feature, "MediaSourceAbortRemove", kUnknown,
          "Using SourceBuffer.abort() to abort remove()'s asynchronous range "
          "removal is deprecated due to specification change. Support will be "
          "removed in the future. You should instead await 'updateend'. "
          "abort() is intended to only abort an asynchronous media append or "
          "reset parser state.",
          "6107495151960064");

    case WebFeature::kMediaSourceDurationTruncatingBuffered:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          feature, "MediaSourceDurationTruncatingBuffered", kUnknown,
          "Setting MediaSource.duration below the highest presentation "
          "timestamp of any buffered coded frames is deprecated due to "
          "specification change. Support for implicit removal of truncated "
          "buffered media will be removed in the future. You should instead "
          "perform explicit remove(newDuration, oldDuration) on all "
          "sourceBuffers, where newDuration < oldDuration.",
          "6107495151960064");

    case WebFeature::kNotificationInsecureOrigin:
    case WebFeature::kNotificationAPIInsecureOriginIframe:
    case WebFeature::kNotificationPermissionRequestedInsecureOrigin:
      return DeprecationInfo::WithTranslation(
          feature, DeprecationIssueType::kNotificationInsecureOrigin);

    case WebFeature::kNotificationPermissionRequestedIframe:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          feature, "NotificationPermissionRequestedIframe", kUnknown,
          "Permission for the Notification API may no longer be requested from "
          "a cross-origin iframe. You should consider requesting permission "
          "from a top-level frame or opening a new window instead.",
          "6451284559265792");

    case WebFeature::kBatteryStatusInsecureOrigin:
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          feature, "BatteryStatusInsecureOrigin", Milestone::kM103,
          "Using the Battery Status API (e.g. navigator.getBattery()) in "
          "insecure origins like HTTP",
          "4878376799043584");

    case WebFeature::kCSSSelectorInternalMediaControlsOverlayCastButton:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          feature, "CSSSelectorInternalMediaControlsOverlayCastButton",
          kUnknown,
          "The disableRemotePlayback attribute should be used in order to "
          "disable the default Cast integration instead of using "
          "-internal-media-controls-overlay-cast-button selector.",
          "5714245488476160");

    case WebFeature::kSelectionAddRangeIntersect:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          feature, "SelectionAddRangeIntersect", kUnknown,
          "The behavior that Selection.addRange() merges existing Range and "
          "the specified Range was removed.",
          "6680566019653632");

    case WebFeature::kRtcpMuxPolicyNegotiate:
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          feature, "RtcpMuxPolicyNegotiate", kM62, "The rtcpMuxPolicy option",
          "5654810086866944");

    case WebFeature::kCanRequestURLHTTPContainingNewline:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          feature, "CanRequestURLHTTPContainingNewline", kUnknown,
          "Resource requests whose URLs contained both removed whitespace "
          "(`\\n`, `\\r`, `\\t`) characters and less-than characters (`<`) are "
          "blocked. Please remove newlines and encode less-than characters "
          "from places like element attribute values in order to load these "
          "resources.",
          "5735596811091968");

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
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          feature, "ChromeLoadTimesConnectionInfo", kUnknown,
          "chrome.loadTimes() is deprecated, instead use standardized API: "
          "Navigation Timing 2.",
          "5637885046816768");

    case WebFeature::kChromeLoadTimesFirstPaintTime:
    case WebFeature::kChromeLoadTimesFirstPaintAfterLoadTime:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          feature, "ChromeLoadTimesFirstPaintAfterLoadTime", kUnknown,
          "chrome.loadTimes() is deprecated, instead use standardized API: "
          "Paint Timing.",
          "5637885046816768");

    case WebFeature::kChromeLoadTimesWasFetchedViaSpdy:
    case WebFeature::kChromeLoadTimesWasNpnNegotiated:
    case WebFeature::kChromeLoadTimesNpnNegotiatedProtocol:
    case WebFeature::kChromeLoadTimesWasAlternateProtocolAvailable:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          feature, "ChromeLoadTimesWasAlternateProtocolAvailable", kUnknown,
          "chrome.loadTimes() is deprecated, instead use standardized API: "
          "nextHopProtocol in Navigation Timing 2.",
          "5637885046816768");

    case WebFeature::kMediaElementSourceOnOfflineContext:
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          feature, "MediaElementAudioSourceNode", kM71,
          "Creating a MediaElementAudioSourceNode on an OfflineAudioContext",
          "5258622686724096");

    case WebFeature::kTextToSpeech_SpeakDisallowedByAutoplay:
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          feature, "TextToSpeech_DisallowedByAutoplay", kM71,
          "speechSynthesis.speak() without user activation",
          "5687444770914304");

    case WebFeature::kRTCPeerConnectionComplexPlanBSdpUsingDefaultSdpSemantics:
      return DeprecationInfo::WithTranslation(
          feature,
          DeprecationIssueType::
              kRTCPeerConnectionComplexPlanBSdpUsingDefaultSdpSemantics);

    case WebFeature::kNoSysexWebMIDIWithoutPermission:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          feature, "NoSysexWebMIDIWithoutPermission", kM82,
          String::Format(
              "Web MIDI will ask a permission to use even if the sysex is not "
              "specified in the MIDIOptions since around %s.",
              MilestoneString(kM82).Ascii().c_str()),
          "5138066234671104");

    case WebFeature::kCustomCursorIntersectsViewport:
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          feature, "CustomCursorIntersectsViewport", kM75,
          "Custom cursors with size greater than 32x32 DIP intersecting native "
          "UI",
          "5825971391299584");

    case WebFeature::kXRSupportsSession:
      return DeprecationInfo::WithFeatureAndReplacement(
          feature, "XRSupportsSession", kM80, "supportsSession()",
          "isSessionSupported() and check the resolved boolean value");

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
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          feature, "RTCPeerConnectionSdpSemanticsPlanB", kM93,
          "Plan B SDP semantics, which is used when constructing an "
          "RTCPeerConnection with {sdpSemantics:\"plan-b\"}, is a legacy "
          "non-standard version of the Session Description Protocol that has "
          "been permanently deleted from the Web Platform. It is still "
          "available when building with IS_FUCHSIA, but we intend to delete it "
          "as soon as possible. Stop depending on it. See "
          "https://crbug.com/1302249 for status.",
          "5823036655665152");

    case WebFeature::kAddressSpacePublicNonSecureContextEmbeddedPrivate:
    case WebFeature::kAddressSpacePublicNonSecureContextEmbeddedLocal:
    case WebFeature::kAddressSpacePrivateNonSecureContextEmbeddedLocal:
      return DeprecationInfo::WithDetailsAndChromeStatusID(
          feature, "InsecurePrivateNetworkSubresourceRequest", kM92,
          "The website requested a subresource from a network that it could "
          "only access because of its users' privileged network position. "
          "These requests expose non-public devices and servers to the "
          "internet, increasing the risk of a cross-site request forgery "
          "(CSRF) attack, and/or information leakage. To mitigate these risks, "
          "Chrome deprecates requests to non-public subresources when "
          "initiated from non-secure contexts, and will start blocking them in "
          "Chrome 92 (July 2021).",
          "5436853517811712");
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
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          feature, "PaymentRequestBasicCard", kM100,
          "The 'basic-card' payment method", "5730051011117056");

    case WebFeature::kPaymentRequestShowWithoutGesture:
      return DeprecationInfo::WithFeatureAndChromeStatusID(
          feature, "PaymentRequestShowWithoutGesture", kM102,
          "Calling PaymentRequest.show() without user activation",
          "5948593429020672");

    case WebFeature::kHostCandidateAttributeGetter:
      return DeprecationInfo::WithFeatureAndReplacement(
          feature, "HostCandidateAttributeGetter", kUnknown,
          "'RTCPeerConnectionIceErrorEvent.hostCandidate'",
          "'RTCPeerConnectionIceErrorEvent.address', "
          "'RTCPeerConnectionIceErrorEvent.port'");

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
      return DeprecationInfo::WithFeatureAndReplacementAndChromeStatusID(
          feature, "WebFeature::kEventPath", kM109, "'Event.path'",
          "'Event.composedPath()'", "5726124632965120");

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
