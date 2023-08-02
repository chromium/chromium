// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/translate/web_view_translate_ranker_factory.h"

#import <utility>

#import "base/no_destructor.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/translate/core/browser/translate_ranker_impl.h"
#import "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
WebViewTranslateRankerFactory* WebViewTranslateRankerFactory::GetInstance() {
  static base::NoDestructor<WebViewTranslateRankerFactory> instance;
  return instance.get();
}

// static
translate::TranslateRanker* WebViewTranslateRankerFactory::GetForBrowserState(
    WebViewBrowserState* state) {
  return static_cast<translate::TranslateRanker*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

WebViewTranslateRankerFactory::WebViewTranslateRankerFactory()
    : BrowserStateKeyedServiceFactory(
          "TranslateRankerService",
          BrowserStateDependencyManager::GetInstance()) {}

WebViewTranslateRankerFactory::~WebViewTranslateRankerFactory() {}

std::unique_ptr<KeyedService>
WebViewTranslateRankerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* web_view_browser_state =
      WebViewBrowserState::FromBrowserState(context);
  std::unique_ptr<translate::TranslateRankerImpl> ranker =
      std::make_unique<translate::TranslateRankerImpl>(
          translate::TranslateRankerImpl::GetModelPath(
              web_view_browser_state->GetStatePath()),
          translate::TranslateRankerImpl::GetModelURL(),
          nullptr /* ukm::UkmRecorder */);
  // WebView has no consumer of translate ranker events, so don't generate them.
  ranker->EnableLogging(false);
  return ranker;
}

web::BrowserState* WebViewTranslateRankerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return browser_state->GetRecordingBrowserState();
}

}  // namespace ios_web_view
