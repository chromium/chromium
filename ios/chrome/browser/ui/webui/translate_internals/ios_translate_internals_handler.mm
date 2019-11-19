// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/translate_internals/ios_translate_internals_handler.h"

#include "components/translate/core/common/language_detection_details.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#include "ios/chrome/browser/translate/translate_service_ios.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/webui/web_ui_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSTranslateInternalsHandler::IOSTranslateInternalsHandler()
    : scoped_web_state_list_observer_(
          std::make_unique<ScopedObserver<WebStateList, WebStateListObserver>>(
              this)),
      scoped_tab_helper_observer_(
          std::make_unique<ScopedObserver<
              language::IOSLanguageDetectionTabHelper,
              language::IOSLanguageDetectionTabHelper::Observer>>(this)) {}

IOSTranslateInternalsHandler::~IOSTranslateInternalsHandler() {}

translate::TranslateClient* IOSTranslateInternalsHandler::GetTranslateClient() {
  return ChromeIOSTranslateClient::FromWebState(web_ui()->GetWebState());
}

variations::VariationsService*
IOSTranslateInternalsHandler::GetVariationsService() {
  return GetApplicationContext()->GetVariationsService();
}

void IOSTranslateInternalsHandler::RegisterMessageCallback(
    const std::string& message,
    const MessageCallback& callback) {
  web_ui()->RegisterMessageCallback(message, callback);
}

void IOSTranslateInternalsHandler::CallJavascriptFunction(
    const std::string& function_name,
    const std::vector<const base::Value*>& args) {
  web_ui()->CallJavascriptFunction(function_name, args);
}

void IOSTranslateInternalsHandler::RegisterMessages() {
  web::BrowserState* browser_state = web_ui()->GetWebState()->GetBrowserState();
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(browser_state)
          ->GetOriginalChromeBrowserState();
  NSArray<TabModel*>* tab_models =
      TabModelList::GetTabModelsForChromeBrowserState(chrome_browser_state);
  for (TabModel* tab_model in tab_models) {
    scoped_web_state_list_observer_->Add(tab_model.webStateList);
    for (int i = 0; i < tab_model.webStateList->count(); i++) {
      AddLanguageDetectionObserverForWebState(
          tab_model.webStateList->GetWebStateAt(i));
    }
  }

  RegisterMessageCallbacks();
}

void IOSTranslateInternalsHandler::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  AddLanguageDetectionObserverForWebState(web_state);
}

void IOSTranslateInternalsHandler::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  RemoveLanguageDetectionObserverForWebState(old_web_state);
  AddLanguageDetectionObserverForWebState(new_web_state);
}

void IOSTranslateInternalsHandler::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  RemoveLanguageDetectionObserverForWebState(web_state);
}

void IOSTranslateInternalsHandler::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  if (web_ui()->GetWebState()->GetBrowserState()->IsOffTheRecord() ||
      !GetTranslateClient()->IsTranslatableURL(details.url)) {
    return;
  }

  AddLanguageDetectionDetails(details);
}

void IOSTranslateInternalsHandler::IOSLanguageDetectionTabHelperWasDestroyed(
    language::IOSLanguageDetectionTabHelper* tab_helper) {
  // No-op. The IOSLanguageDetectionTabHelper is stopped being observed in
  // WebStateListObserver callbacks.
}

void IOSTranslateInternalsHandler::AddLanguageDetectionObserverForWebState(
    web::WebState* web_state) {
  language::IOSLanguageDetectionTabHelper* tab_helper =
      language::IOSLanguageDetectionTabHelper::FromWebState(web_state);
  if (!scoped_tab_helper_observer_->IsObserving(tab_helper)) {
    scoped_tab_helper_observer_->Add(tab_helper);
  }
}

void IOSTranslateInternalsHandler::RemoveLanguageDetectionObserverForWebState(
    web::WebState* web_state) {
  language::IOSLanguageDetectionTabHelper* tab_helper =
      language::IOSLanguageDetectionTabHelper::FromWebState(web_state);
  scoped_tab_helper_observer_->Remove(tab_helper);
}
