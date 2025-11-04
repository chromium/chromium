// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_java_script_feature.h"

#import "base/feature_list.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/no_destructor.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_event_holder.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"

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
    : JavaScriptFeature(web::ContentWorld::kPageContentWorld,
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
  std::optional<bool> has_webkitdirectory =
      body_dict.FindBool("hasWebkitdirectory");
  std::optional<bool> has_selected_file = body_dict.FindBool("hasSelectedFile");
  std::optional<double> capture = body_dict.FindDouble("capture");
  if (!accept_type || !has_multiple || !has_webkitdirectory ||
      !has_selected_file || !capture) {
    return;
  }
  int accept_type_int = static_cast<int>(*accept_type);
  // See AcceptType enumeration in
  // ios/chrome/browser/web/model/choose_file/resources/choose_file.ts
  if (accept_type_int < 0 || accept_type_int > 9) {
    return;
  }
  int capture_int = static_cast<int>(*capture);
  // See CaptureType enumeration in
  // ios/chrome/browser/web/model/choose_file/resources/choose_file_utils.ts
  if (capture_int < 0 || capture_int > 2) {
    return;
  }

  LogChooseFileEvent(accept_type_int, *has_multiple, *has_selected_file);

  if (base::FeatureList::IsEnabled(kIOSChooseFromDrive) ||
      base::FeatureList::IsEnabled(kIOSCustomFileUploadMenu)) {
    std::vector<std::string> accept_file_extensions = ParseAttributeFromValue(
        body_dict, "fileExtensions", ParseAcceptAttributeFileExtensions);
    std::vector<std::string> accept_mime_types = ParseAttributeFromValue(
        body_dict, "mimeTypes", ParseAcceptAttributeMimeTypes);
    base::UmaHistogramBoolean(
        "IOS.Web.FileInput.EventDropped",
        ChooseFileEventHolder::GetInstance()->HasLastChooseFileEvent());
    CGPoint screen_location = CGPointZero;
    if (const base::Value::Dict* screen_location_dict =
            body_dict.FindDict("screenLocation")) {
      screen_location.x = screen_location_dict->FindDouble("x").value_or(0);
      screen_location.y = screen_location_dict->FindDouble("y").value_or(0);
    }
    if (const std::string* attribute_value =
            body_dict.FindString("pointerType")) {
      if (*attribute_value == "mouse" &&
          !CGPointEqualToPoint(screen_location, CGPointZero)) {
        // If seems that if the pointer type associated with the event is
        // "mouse" then screenX and screenY do not account for the zoom scale
        // and content offset of the document. In this case these coordinates
        // need to be converted to the browser's coordinate system.
        const CGFloat zoom_scale =
            web_state->GetWebViewProxy().scrollViewProxy.zoomScale;
        const CGPoint content_offset =
            web_state->GetWebViewProxy().scrollViewProxy.contentOffset;
        screen_location.x *= zoom_scale;
        screen_location.y *= zoom_scale;
        screen_location.x -= content_offset.x;
        screen_location.y -= content_offset.y;
      }
    }

    ChooseFileEvent event =
        ChooseFileEvent::Builder()
            .SetAllowMultipleFiles(*has_multiple)
            .SetOnlyAllowDirectory(*has_webkitdirectory)
            .SetHasSelectedFile(*has_selected_file)
            .SetAcceptFileExtensions(std::move(accept_file_extensions))
            .SetAcceptMimeTypes(std::move(accept_mime_types))
            .SetWebState(web_state)
            .SetScreenLocation(screen_location)
            .SetCapture(static_cast<ChooseFileCaptureType>(capture_int))
            .Build();
    if (base::FeatureList::IsEnabled(kIOSCustomFileUploadMenu)) {
      ChooseFileTabHelper* tab_helper =
          ChooseFileTabHelper::FromWebState(web_state);
      tab_helper->SetLastChooseFileEvent(std::move(event));
    } else {
      ChooseFileEventHolder::GetInstance()->SetLastChooseFileEvent(
          std::move(event));
    }
  }
}

void ChooseFileJavaScriptFeature::LogChooseFileEvent(int accept_type,
                                                     bool allow_multiple_files,
                                                     bool has_selected_file) {
  base::UmaHistogramEnumeration(
      "IOS.Web.FileInput.Clicked",
      BucketForChooseFileEvent(accept_type, allow_multiple_files));
  base::UmaHistogramEnumeration(
      "IOS.Web.FileInput.ContentState",
      ContentStateFromAttributes(allow_multiple_files, has_selected_file));
}
