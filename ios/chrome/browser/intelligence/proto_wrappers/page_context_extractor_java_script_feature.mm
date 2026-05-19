// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_extractor_java_script_feature.h"

#import <string>

#import "base/functional/bind.h"
#import "base/json/json_reader.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/values.h"
#import "components/autofill/ios/form_util/remote_frame_registration_java_script_feature.h"
#import "ios/chrome/browser/intelligence/features/features.h"
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
constexpr char kOptimizeIPCPlaceholder[] =
    "window.gCrWebPlaceholderPageContextIPCOptimization";
constexpr char kActionableOptimizationPlaceholder[] =
    "window.gCrWebPlaceholderPageContextActionableOptimization";

// Parses the JSON string into a base::Value.
std::optional<base::Value> ParseJSONExtractionResult(
    const std::string& json_string) {
  return base::JSONReader::Read(json_string, base::JSON_PARSE_RFC);
}

// Parses the JSON extraction result asynchronously to avoid blocking the main
// UI thread during heavy JSON parsing. Then continue with the extraction
// process.
void ProcessJSONExtractionResult(
    base::OnceCallback<void(std::optional<base::Value>)> callback,
    const base::Value* result) {
  if (!result) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (result->is_string()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&ParseJSONExtractionResult, result->GetString()),
        std::move(callback));
  } else {
    // WebKit already parsed into the javascript result into a base::Value.
    // DetachData and null still go through this code.
    std::move(callback).Run(result->Clone());
  }
}

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
    base::SysUTF8ToNSString(kOptimizeIPCPlaceholder) :
            IsPageContextIPCOptimizationEnabled() ? @"true" : @"false",
    base::SysUTF8ToNSString(kActionableOptimizationPlaceholder) :
            IsPageContextIPCOptimizationActionableEnabled() ? @"true"
                                                            : @"false",
  };
}

void PageContextExtractorJavaScriptFeature::ExtractPageContext(
    web::WebFrame* frame,
    bool include_cross_origin_frame_content,
    bool use_rich_extraction,
    bool use_rich_extraction_with_actionable,
    bool extract_paid_content,
    bool attempt_paid_content_json_fixing,
    const std::string& nonce,
    base::TimeDelta timeout,
    base::OnceCallback<void(const base::Value*)> callback) {
  // TODO(crbug.com/464503759): Use one single config to pass all the
  // parameters.
  base::ListValue parameters;
  parameters.Append(nonce);
  parameters.Append(include_cross_origin_frame_content);
  parameters.Append(use_rich_extraction);
  parameters.Append(use_rich_extraction_with_actionable);
  parameters.Append(extract_paid_content);
  parameters.Append(attempt_paid_content_json_fixing);
  CallJavaScriptFunction(frame, "pageContextExtractor.extractPageContext",
                         parameters, std::move(callback), timeout);
}

// Extract Page Context using JSON. This is used with PageContextIPCOptimization
// feature.
void PageContextExtractorJavaScriptFeature::ExtractPageContextJSON(
    web::WebFrame* frame,
    bool include_cross_origin_frame_content,
    bool use_rich_extraction,
    bool use_rich_extraction_with_actionable,
    bool extract_paid_content,
    bool attempt_paid_content_json_fixing,
    const std::string& nonce,
    base::TimeDelta timeout,
    base::OnceCallback<void(std::optional<base::Value>)> callback) {
  // TODO(crbug.com/464503759): Use one single config to pass all the
  // parameters.
  base::ListValue parameters;
  parameters.Append(nonce);
  parameters.Append(include_cross_origin_frame_content);
  parameters.Append(use_rich_extraction);
  parameters.Append(use_rich_extraction_with_actionable);
  parameters.Append(extract_paid_content);
  parameters.Append(attempt_paid_content_json_fixing);
  CallJavaScriptFunction(
      frame, "pageContextExtractor.extractPageContext", parameters,
      base::BindOnce(&ProcessJSONExtractionResult, std::move(callback)),
      timeout);
}
