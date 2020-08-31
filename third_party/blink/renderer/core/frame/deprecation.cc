// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/deprecation.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/reporting/reporting.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation_report_body.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"

namespace blink {

namespace {

const char kChromeLoadTimesNavigationTiming[] =
    "chrome.loadTimes() is deprecated, instead use standardized API: "
    "Navigation Timing 2. "
    "https://www.chromestatus.com/features/5637885046816768.";
const char kChromeLoadTimesNextHopProtocol[] =
    "chrome.loadTimes() is deprecated, instead use standardized API: "
    "nextHopProtocol in Navigation Timing 2. "
    "https://www.chromestatus.com/features/5637885046816768.";

const char kChromeLoadTimesPaintTiming[] =
    "chrome.loadTimes() is deprecated, instead use standardized API: "
    "Paint Timing. "
    "https://www.chromestatus.com/features/5637885046816768.";

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
};

// Returns estimated milestone dates as milliseconds since January 1, 1970.
base::Time::Exploded MilestoneDate(Milestone milestone) {
  // These are the Estimated Stable Dates:
  // https://www.chromium.org/developers/calendar
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
      // This release is not yet scheduled, so this date is a guess.
      // https://groups.google.com/a/chromium.org/d/msg/chromium-dev/N1NxbSVOZas/ySlEKDKkBgAJ
      return {2020, 7, 0, 14, 4};
    case kM85:
      // This release is not yet scheduled, so this date is a guess.
      // https://groups.google.com/a/chromium.org/d/msg/chromium-dev/N1NxbSVOZas/ySlEKDKkBgAJ
      return {2020, 8, 0, 25, 4};
    case kM86:
      // This release is not yet scheduled, so this date is a guess.
      // https://groups.google.com/a/chromium.org/d/msg/chromium-dev/N1NxbSVOZas/ySlEKDKkBgAJ
      return {2020, 10, 0, 6, 4};
    case kM87:
      // This release is not yet scheduled, so this date is a guess.
      // https://groups.google.com/a/chromium.org/d/msg/chromium-dev/N1NxbSVOZas/ySlEKDKkBgAJ
      return {2020, 11, 0, 17, 4};
    case kM88:
      // This release is not yet scheduled, so this date is a guess.
      // https://groups.google.com/a/chromium.org/d/msg/chromium-dev/N1NxbSVOZas/ySlEKDKkBgAJ
      return {2021, 1, 0, 19, 4};
  }

  NOTREACHED();
  return {1970, 1, 0, 1, 0};
}

// Returns estimated milestone dates as human-readable strings.
String MilestoneString(Milestone milestone) {
  if (milestone == kUnknown)
    return "";
  base::Time::Exploded date = MilestoneDate(milestone);
  return String::Format("M%d, around %s %d", milestone,
                        WTF::kMonthFullName[date.month - 1], date.year);
}

struct DeprecationInfo {
  String id;
  Milestone anticipated_removal;
  String message;
};

String ReplacedBy(const char* feature, const char* replacement) {
  return String::Format("%s is deprecated. Please use %s instead.", feature,
                        replacement);
}

String WillBeRemoved(const char* feature,
                     Milestone milestone,
                     const char* details) {
  return String::Format(
      "%s is deprecated and will be removed in %s. See "
      "https://www.chromestatus.com/features/%s for more details.",
      feature, MilestoneString(milestone).Ascii().c_str(), details);
}

String ReplacedWillBeRemoved(const char* feature,
                             const char* replacement,
                             Milestone milestone,
                             const char* details) {
  return String::Format(
      "%s is deprecated and will be removed in %s. Please use %s instead. See "
      "https://www.chromestatus.com/features/%s for more details.",
      feature, MilestoneString(milestone).Ascii().c_str(), replacement,
      details);
}

DeprecationInfo GetDeprecationInfo(WebFeature feature) {
  switch (feature) {
    // Quota
    case WebFeature::kPrefixedStorageInfo:
      return {"PrefixedStorageInfo", kUnknown,
              ReplacedBy("'window.webkitStorageInfo'",
                         "'navigator.webkitTemporaryStorage' or "
                         "'navigator.webkitPersistentStorage'")};

    case WebFeature::kPrefixedVideoSupportsFullscreen:
      return {"PrefixedVideoSupportsFullscreen", kUnknown,
              ReplacedBy("'HTMLVideoElement.webkitSupportsFullscreen'",
                         "'Document.fullscreenEnabled'")};

    case WebFeature::kPrefixedVideoDisplayingFullscreen:
      return {"PrefixedVideoDisplayingFullscreen", kUnknown,
              ReplacedBy("'HTMLVideoElement.webkitDisplayingFullscreen'",
                         "'Document.fullscreenElement'")};

    case WebFeature::kPrefixedVideoEnterFullscreen:
      return {"PrefixedVideoEnterFullscreen", kUnknown,
              ReplacedBy("'HTMLVideoElement.webkitEnterFullscreen()'",
                         "'Element.requestFullscreen()'")};

    case WebFeature::kPrefixedVideoExitFullscreen:
      return {"PrefixedVideoExitFullscreen", kUnknown,
              ReplacedBy("'HTMLVideoElement.webkitExitFullscreen()'",
                         "'Document.exitFullscreen()'")};

    case WebFeature::kPrefixedVideoEnterFullScreen:
      return {"PrefixedVideoEnterFullScreen", kUnknown,
              ReplacedBy("'HTMLVideoElement.webkitEnterFullScreen()'",
                         "'Element.requestFullscreen()'")};

    case WebFeature::kPrefixedVideoExitFullScreen:
      return {"PrefixedVideoExitFullScreen", kUnknown,
              ReplacedBy("'HTMLVideoElement.webkitExitFullScreen()'",
                         "'Document.exitFullscreen()'")};

    case WebFeature::kPrefixedRequestAnimationFrame:
      return {"PrefixedRequestAnimationFrame", kUnknown,
              "'webkitRequestAnimationFrame' is vendor-specific. Please use "
              "the standard 'requestAnimationFrame' instead."};

    case WebFeature::kPrefixedCancelAnimationFrame:
      return {"PrefixedCancelAnimationFrame", kUnknown,
              "'webkitCancelAnimationFrame' is vendor-specific. Please use the "
              "standard 'cancelAnimationFrame' instead."};

    case WebFeature::kPictureSourceSrc:
      return {"PictureSourceSrc", kUnknown,
              "<source src> with a <picture> parent is invalid and therefore "
              "ignored. Please use <source srcset> instead."};

    case WebFeature::kXMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload:
      return {"XMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload",
              kUnknown,
              "Synchronous XMLHttpRequest on the main thread is deprecated "
              "because of its detrimental effects to the end user's "
              "experience. For more help, check https://xhr.spec.whatwg.org/."};

    case WebFeature::kPrefixedWindowURL:
      return {"PrefixedWindowURL", kUnknown,
              ReplacedBy("'webkitURL'", "'URL'")};

    case WebFeature::kRangeExpand:
      return {"RangeExpand", kUnknown,
              ReplacedBy("'Range.expand()'", "'Selection.modify()'")};

    // Blocked subresource requests:
    case WebFeature::kLegacyProtocolEmbeddedAsSubresource:
      return {"LegacyProtocolEmbeddedAsSubresource", kUnknown,
              "Subresource requests using legacy protocols (like `ftp:`) "
              "are blocked. Please deliver web-accessible resources over "
              "modern protocols like HTTPS. See "
              "https://www.chromestatus.com/feature/5709390967472128 for "
              "details."};

    case WebFeature::kRequestedSubresourceWithEmbeddedCredentials:
      return {"RequestedSubresourceWithEmbeddedCredentials", kUnknown,
              "Subresource requests whose URLs contain embedded credentials "
              "(e.g. `https://user:pass@host/`) are blocked. See "
              "https://www.chromestatus.com/feature/5669008342777856 for more "
              "details."};

    // Powerful features on insecure origins (https://goo.gl/rStTGz)
    case WebFeature::kGeolocationInsecureOrigin:
    case WebFeature::kGeolocationInsecureOriginIframe:
      return {"GeolocationInsecureOrigin", kUnknown,
              "getCurrentPosition() and watchPosition() no longer work on "
              "insecure origins. To use this feature, you should consider "
              "switching your application to a secure origin, such as HTTPS. "
              "See https://goo.gl/rStTGz for more details."};

    case WebFeature::kGeolocationInsecureOriginDeprecatedNotRemoved:
    case WebFeature::kGeolocationInsecureOriginIframeDeprecatedNotRemoved:
      return {"GeolocationInsecureOriginDeprecatedNotRemoved", kUnknown,
              "getCurrentPosition() and watchPosition() are deprecated on "
              "insecure origins. To use this feature, you should consider "
              "switching your application to a secure origin, such as HTTPS. "
              "See https://goo.gl/rStTGz for more details."};

    case WebFeature::kGetUserMediaInsecureOrigin:
    case WebFeature::kGetUserMediaInsecureOriginIframe:
      return {
          "GetUserMediaInsecureOrigin", kUnknown,
          "getUserMedia() no longer works on insecure origins. To use this "
          "feature, you should consider switching your application to a "
          "secure origin, such as HTTPS. See https://goo.gl/rStTGz for more "
          "details."};

    case WebFeature::kMediaSourceAbortRemove:
      return {
          "MediaSourceAbortRemove", kUnknown,
          "Using SourceBuffer.abort() to abort remove()'s asynchronous "
          "range removal is deprecated due to specification change. Support "
          "will be removed in the future. You should instead await "
          "'updateend'. abort() is intended to only abort an asynchronous "
          "media append or reset parser state. See "
          "https://www.chromestatus.com/features/6107495151960064 for more "
          "details."};

    case WebFeature::kMediaSourceDurationTruncatingBuffered:
      return {
          "MediaSourceDurationTruncatingBuffered", kUnknown,
          "Setting MediaSource.duration below the highest presentation "
          "timestamp of any buffered coded frames is deprecated due to "
          "specification change. Support for implicit removal of truncated "
          "buffered media will be removed in the future. You should instead "
          "perform explicit remove(newDuration, oldDuration) on all "
          "sourceBuffers, where newDuration < oldDuration. See "
          "https://www.chromestatus.com/features/6107495151960064 for more "
          "details."};

    case WebFeature::kApplicationCacheAPIInsecureOrigin:
    case WebFeature::kApplicationCacheManifestSelectInsecureOrigin:
      return {"ApplicationCacheAPIInsecureOrigin", kM70,
              "Application Cache was previously restricted to secure origins "
              "only from M70 on but now secure origin use is deprecated and "
              "will be removed in M82.  Please shift your use case over to "
              "Service Workers."};

    case WebFeature::kApplicationCacheAPISecureOrigin:
      return {
          "ApplicationCacheAPISecureOrigin", kM85,
          WillBeRemoved("Application Cache API use", kM85, "6192449487634432")};

    case WebFeature::kApplicationCacheManifestSelectSecureOrigin:
      return {"ApplicationCacheAPISecureOrigin", kM85,
              WillBeRemoved("Application Cache API manifest selection", kM85,
                            "6192449487634432")};

    case WebFeature::kNotificationInsecureOrigin:
    case WebFeature::kNotificationAPIInsecureOriginIframe:
    case WebFeature::kNotificationPermissionRequestedInsecureOrigin:
      return {"NotificationInsecureOrigin", kUnknown,
              "The Notification API may no longer be used from insecure "
              "origins. "
              "You should consider switching your application to a secure "
              "origin, "
              "such as HTTPS. See https://goo.gl/rStTGz for more details."};

    case WebFeature::kNotificationPermissionRequestedIframe:
      return {"NotificationPermissionRequestedIframe", kUnknown,
              "Permission for the Notification API may no longer be requested "
              "from "
              "a cross-origin iframe. You should consider requesting "
              "permission "
              "from a top-level frame or opening a new window instead. See "
              "https://www.chromestatus.com/feature/6451284559265792 for more "
              "details."};

    case WebFeature::kCSSDeepCombinator:
      return {"CSSDeepCombinator", kM65,
              "/deep/ combinator is no longer supported in CSS dynamic "
              "profile. "
              "It is now effectively no-op, acting as if it were a descendant "
              "combinator. /deep/ combinator will be removed, and will be "
              "invalid at M65. You should remove it. See "
              "https://www.chromestatus.com/features/4964279606312960 for more "
              "details."};

    case WebFeature::kCSSSelectorInternalMediaControlsOverlayCastButton:
      return {"CSSSelectorInternalMediaControlsOverlayCastButton", kUnknown,
              "The disableRemotePlayback attribute should be used in order to "
              "disable the default Cast integration instead of using "
              "-internal-media-controls-overlay-cast-button selector. See "
              "https://www.chromestatus.com/feature/5714245488476160 for more "
              "details."};

    case WebFeature::kSelectionAddRangeIntersect:
      return {
          "SelectionAddRangeIntersect", kUnknown,
          "The behavior that Selection.addRange() merges existing Range and "
          "the specified Range was removed. See "
          "https://www.chromestatus.com/features/6680566019653632 for more "
          "details."};

    case WebFeature::kRtcpMuxPolicyNegotiate:
      return {"RtcpMuxPolicyNegotiate", kM62,
              String::Format("The rtcpMuxPolicy option is being considered for "
                             "removal and may be removed no earlier than %s. "
                             "If you depend on it, "
                             "please see "
                             "https://www.chromestatus.com/features/"
                             "5654810086866944 "
                             "for more details.",
                             MilestoneString(kM62).Ascii().c_str())};

    case WebFeature::kCanRequestURLHTTPContainingNewline:
      return {
          "CanRequestURLHTTPContainingNewline", kUnknown,
          "Resource requests whose URLs contained both removed whitespace "
          "(`\\n`, `\\r`, `\\t`) characters and less-than characters (`<`) "
          "are blocked. Please remove newlines and encode less-than "
          "characters from places like element attribute values in order to "
          "load these resources. See "
          "https://www.chromestatus.com/feature/5735596811091968 for more "
          "details."};

#define kWebComponentsV0DeprecationPost                \
  "https://developers.google.com/web/updates/2019/07/" \
  "web-components-time-to-upgrade"

    case WebFeature::kHTMLImports:
      return {"HTMLImports", kM80,
              ReplacedWillBeRemoved(
                  "HTML Imports", "ES modules", kM80,
                  "5144752345317376 and " kWebComponentsV0DeprecationPost)};

    case WebFeature::kElementCreateShadowRoot:
      return {"ElementCreateShadowRoot", kM80,
              ReplacedWillBeRemoved(
                  "Element.createShadowRoot", "Element.attachShadow", kM80,
                  "4507242028072960 and " kWebComponentsV0DeprecationPost)};

    case WebFeature::kDocumentRegisterElement:
      return {
          "DocumentRegisterElement", kM80,
          ReplacedWillBeRemoved(
              "document.registerElement", "window.customElements.define", kM80,
              "4642138092470272 and " kWebComponentsV0DeprecationPost)};
    case WebFeature::kCSSSelectorPseudoUnresolved:
      return {"CSSSelectorPseudoUnresolved", kM80,
              ReplacedWillBeRemoved(
                  ":unresolved pseudo selector", ":not(:defined)", kM80,
                  "4642138092470272 and " kWebComponentsV0DeprecationPost)};

#undef kWebComponentsV0DeprecationPost

    case WebFeature::kLocalCSSFileExtensionRejected:
      return {"LocalCSSFileExtensionRejected", kM64,
              String("CSS cannot be loaded from `file:` URLs unless they end "
                     "in a `.css` file extension.")};

    case WebFeature::kChromeLoadTimesRequestTime:
    case WebFeature::kChromeLoadTimesStartLoadTime:
    case WebFeature::kChromeLoadTimesCommitLoadTime:
    case WebFeature::kChromeLoadTimesFinishDocumentLoadTime:
    case WebFeature::kChromeLoadTimesFinishLoadTime:
    case WebFeature::kChromeLoadTimesNavigationType:
    case WebFeature::kChromeLoadTimesConnectionInfo:
      return {"ChromeLoadTimesConnectionInfo", kUnknown,
              kChromeLoadTimesNavigationTiming};

    case WebFeature::kChromeLoadTimesFirstPaintTime:
    case WebFeature::kChromeLoadTimesFirstPaintAfterLoadTime:
      return {"ChromeLoadTimesFirstPaintAfterLoadTime", kUnknown,
              kChromeLoadTimesPaintTiming};

    case WebFeature::kChromeLoadTimesWasFetchedViaSpdy:
    case WebFeature::kChromeLoadTimesWasNpnNegotiated:
    case WebFeature::kChromeLoadTimesNpnNegotiatedProtocol:
    case WebFeature::kChromeLoadTimesWasAlternateProtocolAvailable:
      return {"ChromeLoadTimesWasAlternateProtocolAvailable", kUnknown,
              kChromeLoadTimesNextHopProtocol};

    case WebFeature::kMediaElementSourceOnOfflineContext:
      return {"MediaElementAudioSourceNode", kM71,
              WillBeRemoved("Creating a MediaElementAudioSourceNode on an "
                            "OfflineAudioContext",
                            kM71, "5258622686724096")};

    case WebFeature::kMediaStreamDestinationOnOfflineContext:
      return {"MediaStreamAudioDestinationNode", kM71,
              WillBeRemoved("Creating a MediaStreamAudioDestinationNode on an "
                            "OfflineAudioContext",
                            kM71, "5258622686724096")};

    case WebFeature::kMediaStreamSourceOnOfflineContext:
      return {
          "MediaStreamAudioSourceNode", kM71,
          WillBeRemoved(
              "Creating a MediaStreamAudioSourceNode on an OfflineAudioContext",
              kM71, "5258622686724096")};

    case WebFeature::kTextToSpeech_SpeakDisallowedByAutoplay:
      return {
          "TextToSpeech_DisallowedByAutoplay", kM71,
          String::Format("speechSynthesis.speak() without user activation is "
                         "no longer allowed since %s. See "
                         "https://www.chromestatus.com/feature/"
                         "5687444770914304 for more details",
                         MilestoneString(kM71).Ascii().c_str())};

    case WebFeature::kRTCPeerConnectionComplexPlanBSdpUsingDefaultSdpSemantics:
      return {"RTCPeerConnectionComplexPlanBSdpUsingDefaultSdpSemantics", kM72,
              String::Format(
                  "\"Complex\" Plan B SDP detected! Chrome will switch the "
                  "default sdpSemantics in %s from 'plan-b' to the "
                  "standardized 'unified-plan' format and this peer connection "
                  "is relying on the default sdpSemantics. This SDP is not "
                  "compatible with Unified Plan and will be rejected by "
                  "clients expecting Unified Plan. For more information about "
                  "how to prepare for the switch, see "
                  "https://webrtc.org/web-apis/chrome/unified-plan/.",
                  MilestoneString(kM72).Ascii().c_str())};

    case WebFeature::kNoSysexWebMIDIWithoutPermission:
      return {"NoSysexWebMIDIWithoutPermission", kM82,
              String::Format(
                  "Web MIDI will ask a permission to use even if the sysex is "
                  "not specified in the MIDIOptions since around %s. See "
                  "https://www.chromestatus.com/feature/5138066234671104 for "
                  "more details.",
                  MilestoneString(kM82).Ascii().c_str())};

    case WebFeature::kCustomCursorIntersectsViewport:
      return {
          "CustomCursorIntersectsViewport", kM75,
          WillBeRemoved(
              "Custom cursors with size greater than 32x32 DIP intersecting "
              "native UI",
              kM75, "5825971391299584")};

    case WebFeature::kXRSupportsSession:
      return {"XRSupportsSession", kM80,
              ReplacedBy(
                  "supportsSession()",
                  "isSessionSupported() and check the resolved boolean value")};

    case WebFeature::kObsoleteWebrtcTlsVersion:
      return {"ObsoleteWebRtcCipherSuite", kM81,
              String::Format(
                  "Your partner is negotiating an obsolete (D)TLS version. "
                  "Support for this will be removed in %s. "
                  "Please check with your partner to have this fixed.",
                  MilestoneString(kM81).Ascii().c_str())};

    case WebFeature::kCssStyleSheetReplaceWithImport:
      return {
          "CssStyleSheetReplaceWithImport", kM84,
          String::Format(
              "Support for calls to CSSStyleSheet.replace() with stylesheet "
              "text that includes @import has been deprecated, and will be "
              "removed in %s. See "
              "https://chromestatus.com/feature/4735925877735424 for more "
              "details.",
              MilestoneString(kM84).Ascii().c_str())};

    case WebFeature::kV8RTCRtpSender_CreateEncodedAudioStreams_Method:
      return {"V8RTCRtpSender_CreateEncodedAudioStreams_Method", kM88,
              ReplacedWillBeRemoved("RTCRtpSender.createEncodedAudioStreams",
                                    "RTCRtpSender.createEncodedStreams", kM88,
                                    "6321945865879552")};

    case WebFeature::kV8RTCRtpSender_CreateEncodedVideoStreams_Method:
      return {"V8RTCRtpSender_CreateEncodedVideoStreams_Method", kM88,
              ReplacedWillBeRemoved("RTCRtpSender.createEncodedVideoStreams",
                                    "RTCRtpSender.createEncodedStreams", kM88,
                                    "6321945865879552")};

    case WebFeature::kV8RTCRtpReceiver_CreateEncodedAudioStreams_Method:
      return {"V8RTCRtpReceiver_CreateEncodedAudioStreams_Method", kM88,
              ReplacedWillBeRemoved("RTCRtpReceiver.createEncodedAudioStreams",
                                    "RTCRtpReceiver.createEncodedStreams", kM88,
                                    "6321945865879552")};

    case WebFeature::kV8RTCRtpReceiver_CreateEncodedVideoStreams_Method:
      return {"V8RTCRtpReceiver_CreateEncodedVideoStreams_Method", kM88,
              ReplacedWillBeRemoved("RTCRtpReceiver.createEncodedVideoStreams",
                                    "RTCRtpReceiver.createEncodedStreams", kM88,
                                    "6321945865879552")};

    case WebFeature::kForceEncodedAudioInsertableStreams:
      return {"ForceEncodedAudioInsertableStreams", kM88,
              ReplacedWillBeRemoved(
                  "RTCConfiguration.forceEncodedAudioInsertableStreams",
                  "RTCConfiguration.encodedInsertableStreams", kM88,
                  "6321945865879552")};

    case WebFeature::kForceEncodedVideoInsertableStreams:
      return {"ForceEncodedVideoInsertableStreams", kM88,
              ReplacedWillBeRemoved(
                  "RTCConfiguration.forceEncodedVideoInsertableStreams",
                  "RTCConfiguration.encodedInsertableStreams", kM88,
                  "6321945865879552")};

    // Features that aren't deprecated don't have a deprecation message.
    default:
      return {"NotDeprecated", kUnknown, ""};
  }
}

Report* CreateReportInternal(const KURL& context_url,
                             const DeprecationInfo& info) {
  base::Optional<base::Time> optional_removal_date;
  if (info.anticipated_removal != kUnknown) {
    base::Time removal_date;
    bool result = base::Time::FromUTCExploded(
        MilestoneDate(info.anticipated_removal), &removal_date);
    DCHECK(result);
    optional_removal_date = removal_date;
  }
  DeprecationReportBody* body = MakeGarbageCollected<DeprecationReportBody>(
      info.id, optional_removal_date, info.message);
  return MakeGarbageCollected<Report>(ReportType::kDeprecation, context_url,
                                      body);
}

}  // anonymous namespace

Deprecation::Deprecation() : mute_count_(0) {}

void Deprecation::ClearSuppression() {
  css_property_deprecation_bits_.reset();
  features_deprecation_bits_.reset();
}

void Deprecation::MuteForInspector() {
  mute_count_++;
}

void Deprecation::UnmuteForInspector() {
  mute_count_--;
}

void Deprecation::Suppress(CSSPropertyID unresolved_property) {
  DCHECK(isCSSPropertyIDWithName(unresolved_property));
  css_property_deprecation_bits_.set(static_cast<size_t>(unresolved_property));
}

bool Deprecation::IsSuppressed(CSSPropertyID unresolved_property) {
  DCHECK(isCSSPropertyIDWithName(unresolved_property));
  return css_property_deprecation_bits_[static_cast<size_t>(
      unresolved_property)];
}

void Deprecation::SetReported(WebFeature feature) {
  features_deprecation_bits_.set(static_cast<size_t>(feature));
}

bool Deprecation::GetReported(WebFeature feature) const {
  return features_deprecation_bits_[static_cast<size_t>(feature)];
}

void Deprecation::WarnOnDeprecatedProperties(
    const LocalFrame* frame,
    CSSPropertyID unresolved_property) {
  Page* page = frame ? frame->GetPage() : nullptr;
  if (!page || page->GetDeprecation().mute_count_ ||
      page->GetDeprecation().IsSuppressed(unresolved_property))
    return;

  String message = DeprecationMessage(unresolved_property);
  if (!message.IsEmpty()) {
    page->GetDeprecation().Suppress(unresolved_property);
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kDeprecation,
        mojom::ConsoleMessageLevel::kWarning, message);
    frame->Console().AddMessage(console_message);
  }
}

String Deprecation::DeprecationMessage(CSSPropertyID unresolved_property) {
  // TODO: Add a switch here when there are properties that we intend to
  // deprecate.
  // Returning an empty string for now.
  return g_empty_string;
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
    deprecation = &scope->GetDeprecation();
  }

  if (!deprecation || deprecation->mute_count_ ||
      deprecation->GetReported(feature)) {
    return;
  }
  deprecation->SetReported(feature);

  // TODO(yoichio): We should remove these counters when v0 APIs are removed.
  // crbug.com/946875.
  if (feature == WebFeature::kHTMLImports &&
      context->GetOriginTrialContext()->IsFeatureEnabled(
          OriginTrialFeature::kHTMLImports)) {
    context->CountUse(WebFeature::kHTMLImportsOnReverseOriginTrials);
  } else if (feature == WebFeature::kElementCreateShadowRoot &&
             context->GetOriginTrialContext()->IsFeatureEnabled(
                 OriginTrialFeature::kShadowDOMV0)) {
    context->CountUse(
        WebFeature::kElementCreateShadowRootOnReverseOriginTrials);
  } else if (feature == WebFeature::kDocumentRegisterElement &&
             context->GetOriginTrialContext()->IsFeatureEnabled(
                 OriginTrialFeature::kCustomElementsV0)) {
    context->CountUse(
        WebFeature::kDocumentRegisterElementOnReverseOriginTrials);
  }

  // Don't count usage of WebComponentsV0 for chrome:// URLs, but still report
  // the deprecation messages.
  bool count_usage = true;
  if (context->Url().ProtocolIs("chrome") &&
      (feature == WebFeature::kHTMLImports ||
       feature == WebFeature::kElementCreateShadowRoot ||
       feature == WebFeature::kDocumentRegisterElement)) {
    count_usage = false;
  }
  if (count_usage)
    context->CountUse(feature);

  const DeprecationInfo info = GetDeprecationInfo(feature);

  // Send the deprecation message to the console as a warning.
  DCHECK(!info.message.IsEmpty());
  auto* console_message = MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kDeprecation,
      mojom::ConsoleMessageLevel::kWarning, info.message);
  context->AddConsoleMessage(console_message);

  Report* report = CreateReportInternal(context->Url(), info);

  // Send the deprecation report to the Reporting API and any
  // ReportingObservers.
  ReportingContext::From(context)->QueueReport(report);
}

// static
String Deprecation::DeprecationMessage(WebFeature feature) {
  return GetDeprecationInfo(feature).message;
}

}  // namespace blink
