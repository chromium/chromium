// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/image_fetch/image_fetch_java_script_feature.h"

#import <string>

#import "base/base64.h"
#import "base/no_destructor.h"
#import "base/values.h"
#import "ios/chrome/browser/web/image_fetch/image_fetch_tab_helper.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/web_state.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kScriptName[] = "image_fetch";
const char kScriptHandlerName[] = "ImageFetchMessageHandler";

ImageFetchJavaScriptFeature::Handler* GetHandlerFromWebState(
    web::WebState* web_state) {
  return ImageFetchTabHelper::FromWebState(web_state);
}

}  // namespace

ImageFetchJavaScriptFeature::ImageFetchJavaScriptFeature(
    base::RepeatingCallback<Handler*(web::WebState*)> handler_factory)
    : JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {web::java_script_features::GetCommonJavaScriptFeature()}),
      handler_factory_(std::move(handler_factory)) {}

ImageFetchJavaScriptFeature::~ImageFetchJavaScriptFeature() = default;

ImageFetchJavaScriptFeature* ImageFetchJavaScriptFeature::GetInstance() {
  static base::NoDestructor<ImageFetchJavaScriptFeature> instance(
      base::BindRepeating(&GetHandlerFromWebState));

  return instance.get();
}

void ImageFetchJavaScriptFeature::GetImageData(web::WebState* web_state,
                                               int call_id,
                                               const GURL& url) {
  web::WebFrame* main_frame = GetMainFrame(web_state);
  if (!main_frame) {
    return;
  }

  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(call_id));
  parameters.push_back(base::Value(url.spec()));
  CallJavaScriptFunction(main_frame, "imageFetch.getImageData", parameters);
}

absl::optional<std::string>
ImageFetchJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptHandlerName;
}

void ImageFetchJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& script_message) {
  Handler* handler = handler_factory_.Run(web_state);
  if (!handler) {
    return;
  }

  // Verify that the message is well-formed before using it.
  base::Value* message = script_message.body();
  if (!message || !message->is_dict()) {
    return;
  }

  const base::Value::Dict& message_dict = message->GetDict();
  const absl::optional<double> id_key = message_dict.FindDouble("id");
  if (!id_key) {
    return;
  }
  int call_id = static_cast<int>(id_key.value());

  std::string decoded_data;
  const std::string* data = message_dict.FindString("data");
  if (!data || !base::Base64Decode(*data, &decoded_data)) {
    handler->HandleJsFailure(call_id);
    return;
  }

  std::string from;
  const std::string* from_value = message_dict.FindString("from");
  if (from_value) {
    from = *from_value;
  }

  DCHECK(!decoded_data.empty());
  handler->HandleJsSuccess(call_id, decoded_data, from);
}
