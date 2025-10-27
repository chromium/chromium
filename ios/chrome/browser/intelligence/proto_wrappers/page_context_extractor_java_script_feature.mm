// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_extractor_java_script_feature.h"

#import <string>

#import "base/functional/bind.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"

namespace {
constexpr char kScriptName[] = "page_context_extractor";
constexpr char kDetachLogicPlaceholder[] =
    "/*! {{PLACEHOLDER_FOR_DETACH_LOGIC}} */";
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
                  kDocumentEnd,
              web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames,
              web::JavaScriptFeature::FeatureScript::ReinjectionBehavior::
                  kInjectOncePerWindow,
              base::BindRepeating(
                  &PageContextExtractorJavaScriptFeature::GetReplacements,
                  base::Unretained(this)))},
          {web::java_script_features::GetCommonJavaScriptFeature()}) {}

PageContextExtractorJavaScriptFeature::
    ~PageContextExtractorJavaScriptFeature() = default;

web::JavaScriptFeature::FeatureScript::PlaceholderReplacements
PageContextExtractorJavaScriptFeature::GetReplacements() {
  std::u16string full_script_block =
      base::StrCat({u"const SHOULD_DETACH_PAGE_CONTEXT = () => { ",
                    ios::provider::GetPageContextShouldDetachScript(), u" };"});
  return @{
    base::SysUTF8ToNSString(kDetachLogicPlaceholder) :
        base::SysUTF16ToNSString(full_script_block),

  };
}

void PageContextExtractorJavaScriptFeature::ExtractPageContext(
    web::WebFrame* frame,
    bool include_anchors,
    const std::string& nonce,
    base::TimeDelta timeout,
    base::OnceCallback<void(const base::Value*)> callback) {
  base::Value::List parameters;
  parameters.Append(include_anchors);
  parameters.Append(nonce);
  CallJavaScriptFunction(frame, "pageContextExtractor.extractPageContext",
                         parameters, std::move(callback), timeout);
}
