// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/search_engine_java_script_feature.h"

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {
constexpr char kScriptName[] = "search_engine";

constexpr char kSearchEngineMessageHandlerName[] = "SearchEngineMessage";

constexpr char kScriptMessageResponseCommandKey[] = "command";

constexpr char kOpenSearchCommand[] = "openSearch";
constexpr char kOpenSearchCommandPageUrlKey[] = "pageUrl";
constexpr char kOpenSearchCommandOsddUrlKey[] = "osddUrl";

constexpr char kSearchableUrlCommand[] = "searchableUrl";
constexpr char kSearchableUrlCommandUrlKey[] = "url";
}  // namespace

// static
SearchEngineJavaScriptFeature* SearchEngineJavaScriptFeature::GetInstance() {
  static base::NoDestructor<SearchEngineJavaScriptFeature> instance;
  return instance.get();
}

SearchEngineJavaScriptFeature::SearchEngineJavaScriptFeature()
    : JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::
                  kReinjectOnDocumentRecreation)},
          {web::java_script_features::GetCommonJavaScriptFeature()}) {}

SearchEngineJavaScriptFeature::~SearchEngineJavaScriptFeature() = default;

std::optional<std::string>
SearchEngineJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kSearchEngineMessageHandlerName;
}

void SearchEngineJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& script_message) {
  if (!delegate_ || !script_message.body()) {
    return;
  }
  const auto* dict = script_message.body()->GetIfDict();
  if (!dict) {
    return;
  }

  const std::string* command =
      dict->FindString(kScriptMessageResponseCommandKey);
  if (!command) {
    return;
  }

  if (*command == kOpenSearchCommand) {
    const std::string* document_url =
        dict->FindString(kOpenSearchCommandPageUrlKey);
    const std::string* osdd_url =
        dict->FindString(kOpenSearchCommandOsddUrlKey);
    if (!document_url || !osdd_url) {
      return;
    }

    delegate_->AddTemplateURLByOSDD(web_state, GURL(*document_url),
                                    GURL(*osdd_url));
  } else if (*command == kSearchableUrlCommand) {
    const std::string* url = dict->FindString(kSearchableUrlCommandUrlKey);
    if (!url) {
      return;
    }
    delegate_->SetSearchableUrl(web_state, GURL(*url));
  }
}

void SearchEngineJavaScriptFeature::SetDelegate(
    SearchEngineJavaScriptFeatureDelegate* delegate) {
  delegate_ = delegate;
}
