// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_extractor_java_script_feature.h"

#import <string>

#import "base/functional/bind.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/autofill/ios/form_util/remote_frame_registration_java_script_feature.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"

// TODO(crbug.com/458081684): Move away from all autofill dependencies once
// the migration in ios/web is done for frame registration.

namespace {
constexpr char kScriptName[] = "page_context_extractor";
constexpr char kDetachLogicPlaceholder[] =
    "window.gCrWebPlaceholderPageContextShouldDetach";
}  // namespace

PageContextExtractorJavaScriptFeature*
PageContextExtractorJavaScriptFeature::GetInstance() {
  static base::NoDestructor<PageContextExtractorJavaScriptFeature> instance;
  return instance.get();
}

PageContextExtractorJavaScriptFeature::PageContextExtractorJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {web::JavaScriptFeature::FeatureScript::CreateWithFilename(
              kScriptName,
              web::JavaScriptFeature::FeatureScript::InjectionTime::
                  kDocumentStart,
              web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames,
              web::JavaScriptFeature::FeatureScript::ReinjectionBehavior::
                  kInjectOncePerWindow,
              base::BindRepeating(
                  &PageContextExtractorJavaScriptFeature::GetReplacements,
                  base::Unretained(this)))},
          {autofill::RemoteFrameRegistrationJavaScriptFeature::GetInstance()}) {
}

PageContextExtractorJavaScriptFeature::
    ~PageContextExtractorJavaScriptFeature() = default;

web::JavaScriptFeature::FeatureScript::PlaceholderReplacements
PageContextExtractorJavaScriptFeature::GetReplacements() {
  std::u16string detach_script_block =
      base::StrCat({u"(() => { ",
                    ios::provider::GetPageContextShouldDetachScript(), u" })"});
  return @{
    base::SysUTF8ToNSString(kDetachLogicPlaceholder) :
        base::SysUTF16ToNSString(detach_script_block),
  };
}

void PageContextExtractorJavaScriptFeature::ExtractPageContext(
    web::WebFrame* frame,
    bool include_anchors,
    bool include_cross_origin_frame_content,
    bool use_rich_extraction,
    const std::string& nonce,
    base::TimeDelta timeout,
    base::OnceCallback<void(const base::Value*)> callback) {
  // TODO(crbug.com/464503759): Use one single config to pass all the
  // parameters.
  base::ListValue parameters;
  parameters.Append(include_anchors);
  parameters.Append(nonce);
  parameters.Append(include_cross_origin_frame_content);
  parameters.Append(use_rich_extraction);
  CallJavaScriptFunction(frame, "pageContextExtractor.extractPageContext",
                         parameters, std::move(callback), timeout);
}
