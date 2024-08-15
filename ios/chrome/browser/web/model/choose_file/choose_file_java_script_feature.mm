// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_java_script_feature.h"

#import "base/feature_list.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/no_destructor.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_util.h"
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
// ios/chrome/browser/web/model/choose_file/resources/choose_file.ts
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
        NOTREACHED();
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
      NOTREACHED();
  }
}

// Applies `parse_function` to the attribute value associated with
// `attribute_name` in `dict`. If there is no such attribute in `dict`, returns
// an empty vector.
using ParseFunction = std::vector<std::string> (*)(std::string_view);
std::vector<std::string> ParseAttributeFromValue(
    const base::Value::Dict& dict,
    std::string_view attribute_name,
    ParseFunction parse_function) {
  if (const std::string* attribute_value = dict.FindString(attribute_name)) {
    return parse_function(*attribute_value);
  }
  return {};
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

std::optional<std::string>
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

  std::optional<double> accept_type = body_dict.FindDouble("acceptType");
  std::optional<bool> has_multiple = body_dict.FindBool("hasMultiple");
  if (!accept_type || !has_multiple) {
    return;
  }
  int accept_type_int = static_cast<int>(*accept_type);
  // See AcceptType enumeration in
  // ios/chrome/browser/web/model/choose_file/resources/choose_file.ts
  if (accept_type_int < 0 || accept_type_int > 9) {
    return;
  }

  LogChooseFileEvent(accept_type_int, *has_multiple);

  if (base::FeatureList::IsEnabled(kIOSChooseFromDrive)) {
    std::vector<std::string> accept_file_extensions = ParseAttributeFromValue(
        body_dict, "fileExtensions", ParseAcceptAttributeFileExtensions);
    std::vector<std::string> accept_mime_types = ParseAttributeFromValue(
        body_dict, "mimeTypes", ParseAcceptAttributeMimeTypes);
    base::UmaHistogramBoolean("IOS.Web.FileInput.EventDropped",
                              last_choose_file_event_.has_value());
    last_choose_file_event_ = std::make_optional<ChooseFileEvent>(
        *has_multiple, std::move(accept_file_extensions),
        std::move(accept_mime_types), web_state);
  }
}

void ChooseFileJavaScriptFeature::LogChooseFileEvent(
    int accept_type,
    bool allow_multiple_files) {
  base::UmaHistogramEnumeration(
      "IOS.Web.FileInput.Clicked",
      BucketForChooseFileEvent(accept_type, allow_multiple_files));
}

std::optional<ChooseFileEvent>
ChooseFileJavaScriptFeature::ResetLastChooseFileEvent() {
  return std::exchange(last_choose_file_event_, std::nullopt);
}
