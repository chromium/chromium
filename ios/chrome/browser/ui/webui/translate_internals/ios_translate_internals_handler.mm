// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/translate_internals/ios_translate_internals_handler.h"

#import "components/translate/core/common/language_detection_details.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/all_web_state_list_observation_registrar.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/translate/translate_service_ios.h"
#import "ios/web/public/webui/web_ui_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSTranslateInternalsHandler::IOSTranslateInternalsHandler() = default;
IOSTranslateInternalsHandler::~IOSTranslateInternalsHandler() = default;

translate::TranslateClient* IOSTranslateInternalsHandler::GetTranslateClient() {
  return ChromeIOSTranslateClient::FromWebState(web_ui()->GetWebState());
}

variations::VariationsService*
IOSTranslateInternalsHandler::GetVariationsService() {
  return GetApplicationContext()->GetVariationsService();
}

void IOSTranslateInternalsHandler::RegisterMessageCallback(
    base::StringPiece message,
    MessageCallback callback) {
  web_ui()->RegisterMessageCallback(message, std::move(callback));
}

void IOSTranslateInternalsHandler::CallJavascriptFunction(
    base::StringPiece function_name,
    base::span<const base::ValueView> args) {
  web_ui()->CallJavascriptFunction(function_name, args);
}

void IOSTranslateInternalsHandler::RegisterMessages() {
  web::BrowserState* browser_state = web_ui()->GetWebState()->GetBrowserState();
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state)
          ->GetOriginalChromeBrowserState();

  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(chrome_browser_state);
  std::set<Browser*> browsers = chrome_browser_state->IsOffTheRecord()
                                    ? browser_list->AllIncognitoBrowsers()
                                    : browser_list->AllRegularBrowsers();

  for (Browser* browser : browsers) {
    WebStateList* web_state_list = browser->GetWebStateList();
    for (int i = 0; i < web_state_list->count(); i++) {
      AddLanguageDetectionObserverForWebState(web_state_list->GetWebStateAt(i));
    }
  }

  AllWebStateListObservationRegistrar::Mode mode =
      chrome_browser_state->IsOffTheRecord()
          ? AllWebStateListObservationRegistrar::Mode::INCOGNITO
          : AllWebStateListObservationRegistrar::Mode::REGULAR;
  registrar_ = std::make_unique<AllWebStateListObservationRegistrar>(
      browser_list, std::make_unique<Observer>(this), mode);

  RegisterMessageCallbacks();
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
  if (!scoped_tab_helper_observations_.IsObservingSource(tab_helper)) {
    scoped_tab_helper_observations_.AddObservation(tab_helper);
  }
}

void IOSTranslateInternalsHandler::RemoveLanguageDetectionObserverForWebState(
    web::WebState* web_state) {
  language::IOSLanguageDetectionTabHelper* tab_helper =
      language::IOSLanguageDetectionTabHelper::FromWebState(web_state);
  scoped_tab_helper_observations_.RemoveObservation(tab_helper);
}

IOSTranslateInternalsHandler::Observer::Observer(
    IOSTranslateInternalsHandler* handler)
    : handler_(handler) {}
IOSTranslateInternalsHandler::Observer::~Observer() {}

void IOSTranslateInternalsHandler::Observer::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  handler_->AddLanguageDetectionObserverForWebState(web_state);
}

void IOSTranslateInternalsHandler::Observer::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  handler_->RemoveLanguageDetectionObserverForWebState(old_web_state);
  handler_->AddLanguageDetectionObserverForWebState(new_web_state);
}

void IOSTranslateInternalsHandler::Observer::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  handler_->RemoveLanguageDetectionObserverForWebState(web_state);
}
