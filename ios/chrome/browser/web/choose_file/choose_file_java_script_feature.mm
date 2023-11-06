// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/choose_file/choose_file_java_script_feature.h"

#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {
const char kChooseFileScript[] = "choose_file";
const char kChooseFileScriptName[] = "ChooseFileHandler";

// The type of attributes of the input element.
// This enum is persisted in log, do not reorder or reuse buckets.
// Used in IOSWebFileInputAttributes enum for IOS.Web.FileInput.Clicked.
enum class ChooseFileAccept {
  kNoAccept = 0,
  kNoAcceptMultiple = 1,
  kMixedAccept = 2,
  kMixedAcceptMultiple = 3,
  kUnknownAccept = 4,
  kUnknownAcceptMultiple = 5,
  kImageAccept = 6,
  kImageAcceptMultiple = 7,
  kVideoAccept = 8,
  kVideoAcceptMultiple = 9,
  kAudioAccept = 10,
  kAudioAcceptMultiple = 11,
  kArchiveAccept = 12,
  kArchiveAcceptMultiple = 13,
  kPDFAccept = 14,
  kPDFAcceptMultiple = 15,
  kDocAccept = 16,
  kDocAcceptMultiple = 17,
  kAppleAccept = 18,
  kAppleAcceptMultiple = 19,
  kMaxValue = kAppleAcceptMultiple,
};

// The UMA bucket for `accept_type` and `allow_multiple_files`.
// See AcceptType enumeration in
// ios/chrome/browser/web/choose_file/resources/choose_file.ts
ChooseFileAccept BucketForChooseFileEvent(int accept_type,
                                          bool allow_multiple_files) {
  if (allow_multiple_files) {
    switch (accept_type) {
      case 0:
        return ChooseFileAccept::kNoAcceptMultiple;
      case 1:
        return ChooseFileAccept::kMixedAcceptMultiple;
      case 2:
        return ChooseFileAccept::kUnknownAcceptMultiple;
      case 3:
        return ChooseFileAccept::kImageAcceptMultiple;
      case 4:
        return ChooseFileAccept::kVideoAcceptMultiple;
      case 5:
        return ChooseFileAccept::kAudioAcceptMultiple;
      case 6:
        return ChooseFileAccept::kArchiveAcceptMultiple;
      case 7:
        return ChooseFileAccept::kPDFAcceptMultiple;
      case 8:
        return ChooseFileAccept::kDocAcceptMultiple;
      case 9:
        return ChooseFileAccept::kAppleAcceptMultiple;
      default:
        NOTREACHED_NORETURN();
    }
  }
  switch (accept_type) {
    case 0:
      return ChooseFileAccept::kNoAccept;
    case 1:
      return ChooseFileAccept::kMixedAccept;
    case 2:
      return ChooseFileAccept::kUnknownAccept;
    case 3:
      return ChooseFileAccept::kImageAccept;
    case 4:
      return ChooseFileAccept::kVideoAccept;
    case 5:
      return ChooseFileAccept::kAudioAccept;
    case 6:
      return ChooseFileAccept::kArchiveAccept;
    case 7:
      return ChooseFileAccept::kPDFAccept;
    case 8:
      return ChooseFileAccept::kDocAccept;
    case 9:
      return ChooseFileAccept::kAppleAccept;
    default:
      NOTREACHED_NORETURN();
  }
}
}  // namespace

ChooseFileJavaScriptFeature::ChooseFileJavaScriptFeature()
    : JavaScriptFeature(web::ContentWorld::kIsolatedWorld,
                        {FeatureScript::CreateWithFilename(
                            kChooseFileScript,
                            FeatureScript::InjectionTime::kDocumentEnd,
                            FeatureScript::TargetFrames::kAllFrames)}) {}

ChooseFileJavaScriptFeature::~ChooseFileJavaScriptFeature() = default;

ChooseFileJavaScriptFeature* ChooseFileJavaScriptFeature::GetInstance() {
  static base::NoDestructor<ChooseFileJavaScriptFeature> instance;
  return instance.get();
}

absl::optional<std::string>
ChooseFileJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kChooseFileScriptName;
}

void ChooseFileJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  DCHECK(web_state);

  // Verify that the message is well-formed before using it
  if (!message.body()->is_dict()) {
    return;
  }
  base::Value::Dict& body_dict = message.body()->GetDict();

  absl::optional<double> accept_type = body_dict.FindDouble("acceptType");
  absl::optional<bool> has_multiple = body_dict.FindBool("hasMultiple");
  if (!accept_type || !has_multiple) {
    return;
  }
  int accept_type_int = static_cast<int>(*accept_type);
  // See AcceptType enumeration in
  // ios/chrome/browser/web/choose_file/resources/choose_file.ts
  if (accept_type_int < 0 || accept_type_int > 9) {
    return;
  }

  LogChooseFileEvent(accept_type_int, *has_multiple);
}

void ChooseFileJavaScriptFeature::LogChooseFileEvent(
    int accept_type,
    bool allow_multiple_files) {
  base::UmaHistogramEnumeration(
      "IOS.Web.FileInput.Clicked",
      BucketForChooseFileEvent(accept_type, allow_multiple_files));
}
