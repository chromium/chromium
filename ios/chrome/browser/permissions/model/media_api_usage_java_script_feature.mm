// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/permissions/model/media_api_usage_java_script_feature.h"

#import <AVFAudio/AVFAudio.h>

#import "base/metrics/histogram_functions.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "MediaAPIParams" in src/tools/metrics/histograms/enums.xml.
enum class MediaAPIParams {
  kUnknown = 0,
  kAudioOnly = 1,
  kVideoOnly = 2,
  kAudioAndVideo = 3,
  kMaxValue = kAudioAndVideo,
};

static constexpr char kScriptName[] = "media_overrides";

static constexpr char kMediaAPIAccessedHandlerName[] =
    "MediaAPIAccessedHandler";

static constexpr char kScriptMessageResponseAudioKey[] = "audio";
static constexpr char kScriptMessageResponseVideoKey[] = "video";

static constexpr char kMediaAPIAccessedHistogramDenied[] =
    "IOS.JavaScript.Permissions.Media.Denied";
static constexpr char kMediaAPIAccessedHistogramGranted[] =
    "IOS.JavaScript.Permissions.Media.Granted";
static constexpr char kMediaAPIAccessedHistogramUndetermined[] =
    "IOS.JavaScript.Permissions.Media.Undetermined";

}  // namespace

// static
MediaAPIUsageJavaScriptFeature* MediaAPIUsageJavaScriptFeature::GetInstance() {
  static base::NoDestructor<MediaAPIUsageJavaScriptFeature> instance;
  return instance.get();
}

// static
bool MediaAPIUsageJavaScriptFeature::ShouldOverrideAPI() {
  // Install JS overrides if access is `...Undetermined` or `...Denied`.
  if (@available(iOS 17.0, *)) {
    return [AVAudioApplication sharedInstance].recordPermission !=
           AVAudioApplicationRecordPermissionGranted;
  }
  return [AVAudioSession sharedInstance].recordPermission !=
         AVAudioSessionRecordPermissionGranted;
}

MediaAPIUsageJavaScriptFeature::MediaAPIUsageJavaScriptFeature()
    : JavaScriptFeature(web::ContentWorld::kPageContentWorld,
                        {FeatureScript::CreateWithFilename(
                            kScriptName,
                            FeatureScript::InjectionTime::kDocumentStart,
                            FeatureScript::TargetFrames::kAllFrames,
                            FeatureScript::ReinjectionBehavior::
                                kReinjectOnDocumentRecreation)}) {}
MediaAPIUsageJavaScriptFeature::~MediaAPIUsageJavaScriptFeature() = default;

std::optional<std::string>
MediaAPIUsageJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kMediaAPIAccessedHandlerName;
}

void MediaAPIUsageJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& script_message) {
  std::optional<bool> audio;
  std::optional<bool> video;
  const base::Value::Dict* script_dict =
      script_message.body() ? script_message.body()->GetIfDict() : nullptr;
  if (script_dict) {
    audio = script_dict->FindBool(kScriptMessageResponseAudioKey);
    video = script_dict->FindBool(kScriptMessageResponseVideoKey);
  }

  std::string metric_name;
  if (@available(iOS 17.0, *)) {
    switch ([AVAudioApplication sharedInstance].recordPermission) {
      case AVAudioApplicationRecordPermissionDenied:
        metric_name = kMediaAPIAccessedHistogramDenied;
        break;
      case AVAudioApplicationRecordPermissionGranted:
        metric_name = kMediaAPIAccessedHistogramGranted;
        break;
      case AVAudioApplicationRecordPermissionUndetermined:
        metric_name = kMediaAPIAccessedHistogramUndetermined;
        break;
    }
  } else {
    switch ([AVAudioSession sharedInstance].recordPermission) {
      case AVAudioSessionRecordPermissionDenied:
        metric_name = kMediaAPIAccessedHistogramDenied;
        break;
      case AVAudioSessionRecordPermissionGranted:
        metric_name = kMediaAPIAccessedHistogramGranted;
        break;
      case AVAudioSessionRecordPermissionUndetermined:
        metric_name = kMediaAPIAccessedHistogramUndetermined;
        break;
    }
  }

  if (!audio || !video) {
    base::UmaHistogramEnumeration(metric_name, MediaAPIParams::kUnknown);
  } else if (audio.value() && !video.value()) {
    base::UmaHistogramEnumeration(metric_name, MediaAPIParams::kAudioOnly);
  } else if (!audio.value() && video.value()) {
    base::UmaHistogramEnumeration(metric_name, MediaAPIParams::kVideoOnly);
  } else if (audio.value() && video.value()) {
    base::UmaHistogramEnumeration(metric_name, MediaAPIParams::kAudioAndVideo);
  }
}
