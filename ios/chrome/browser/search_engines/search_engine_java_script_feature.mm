// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/search_engine_java_script_feature.h"

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kScriptName[] = "search_engine";

const char kSearchEngineMessageHandlerName[] = "SearchEngineMessage";

static const char kScriptMessageResponseCommandKey[] = "command";

static const char kOpenSearchCommand[] = "openSearch";
static const char kOpenSearchCommandPageUrlKey[] = "pageUrl";
static const char kOpenSearchCommandOsddUrlKey[] = "osddUrl";

static const char kSearchableUrlCommand[] = "searchableUrl";
static const char kSearchableUrlCommandUrlKey[] = "url";
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

absl::optional<std::string>
SearchEngineJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kSearchEngineMessageHandlerName;
}

void SearchEngineJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& script_message) {
  if (!delegate_ || !script_message.body() ||
      !script_message.body()->is_dict()) {
    return;
  }

  std::string* command =
      script_message.body()->FindStringKey(kScriptMessageResponseCommandKey);
  if (!command) {
    return;
  }

  if (*command == kOpenSearchCommand) {
    std::string* document_url =
        script_message.body()->FindStringKey(kOpenSearchCommandPageUrlKey);
    std::string* osdd_url =
        script_message.body()->FindStringKey(kOpenSearchCommandOsddUrlKey);
    if (!document_url || !osdd_url) {
      return;
    }

    delegate_->AddTemplateURLByOSDD(web_state, GURL(*document_url),
                                    GURL(*osdd_url));
  } else if (*command == kSearchableUrlCommand) {
    std::string* url =
        script_message.body()->FindStringKey(kSearchableUrlCommandUrlKey);
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
