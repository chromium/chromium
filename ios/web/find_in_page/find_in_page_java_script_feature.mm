// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/find_in_page/find_in_page_java_script_feature.h"

#import "base/no_destructor.h"
#import "ios/web/find_in_page/find_in_page_constants.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace {
const char kScriptName[] = "find_in_page_native_api";
const char kEventListenersScriptName[] = "find_in_page_event_listeners";

// Timeout for the find within JavaScript in milliseconds.
const double kFindInPageFindTimeout = 100.0;

// The timeout for JavaScript function calls in milliseconds. Important that
// this is longer than `kFindInPageFindTimeout` to allow for incomplete find to
// restart again. If this timeout hits, then something went wrong with the find
// and find in page should not continue.
const double kJavaScriptFunctionCallTimeout = 200.0;
}  // namespace

namespace web {

namespace find_in_page {

const int kFindInPagePending = -1;

}  // namespace find_in_page

// static
FindInPageJavaScriptFeature* FindInPageJavaScriptFeature::GetInstance() {
  static base::NoDestructor<FindInPageJavaScriptFeature> instance;
  return instance.get();
}

FindInPageJavaScriptFeature::FindInPageJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
               kScriptName,
               FeatureScript::InjectionTime::kDocumentStart,
               FeatureScript::TargetFrames::kAllFrames,
               FeatureScript::ReinjectionBehavior::kInjectOncePerWindow),
           FeatureScript::CreateWithFilename(
               kEventListenersScriptName,
               FeatureScript::InjectionTime::kDocumentStart,
               FeatureScript::TargetFrames::kAllFrames,
               FeatureScript::ReinjectionBehavior::
                   kReinjectOnDocumentRecreation)},
          {web::java_script_features::GetBaseJavaScriptFeature()}) {}

FindInPageJavaScriptFeature::~FindInPageJavaScriptFeature() = default;

bool FindInPageJavaScriptFeature::Search(
    WebFrame* frame,
    const std::string& query,
    base::OnceCallback<void(std::optional<int>)> callback) {
  base::Value::List params;
  params.Append(query);
  params.Append(kFindInPageFindTimeout);
  return CallJavaScriptFunction(
      frame, kFindInPageSearch, params,
      base::BindOnce(&FindInPageJavaScriptFeature::ProcessSearchResult,
                     base::Unretained(GetInstance()), std::move(callback)),
      base::Milliseconds(kJavaScriptFunctionCallTimeout));
}

void FindInPageJavaScriptFeature::Pump(
    WebFrame* frame,
    base::OnceCallback<void(std::optional<int>)> callback) {
  base::Value::List params;
  params.Append(kFindInPageFindTimeout);
  CallJavaScriptFunction(
      frame, kFindInPagePump, params,
      base::BindOnce(&FindInPageJavaScriptFeature::ProcessSearchResult,
                     base::Unretained(GetInstance()), std::move(callback)),
      base::Milliseconds(kJavaScriptFunctionCallTimeout));
}

void FindInPageJavaScriptFeature::SelectMatch(
    WebFrame* frame,
    int index,
    base::OnceCallback<void(const base::Value*)> callback) {
  base::Value::List params;
  params.Append(index);
  CallJavaScriptFunction(frame, kFindInPageSelectAndScrollToMatch, params,
                         std::move(callback),
                         base::Milliseconds(kJavaScriptFunctionCallTimeout));
}

void FindInPageJavaScriptFeature::Stop(WebFrame* frame) {
  CallJavaScriptFunction(frame, kFindInPageStop, base::Value::List());
}

void FindInPageJavaScriptFeature::ProcessSearchResult(
    base::OnceCallback<void(const std::optional<int>)> callback,
    const base::Value* result) {
  std::optional<int> match_count;
  if (result && result->is_double()) {
    // Valid match number returned. If not, match count will be 0 in order to
    // zero-out count from previous find.
    match_count = static_cast<int>(result->GetDouble());
  }
  std::move(callback).Run(match_count);
}

}  // namespace web
