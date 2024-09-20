// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/translate_internals/ios_translate_internals_handler.h"

#import <string_view>

#import "components/translate/core/common/language_detection_details.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/all_web_state_list_observation_registrar.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/translate/model/translate_service_ios.h"
#import "ios/web/public/webui/web_ui_ios.h"

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
    std::string_view message,
    MessageCallback callback) {
  web_ui()->RegisterMessageCallback(message, std::move(callback));
}

void IOSTranslateInternalsHandler::CallJavascriptFunction(
    std::string_view function_name,
    base::span<const base::ValueView> args) {
  web_ui()->CallJavascriptFunction(function_name, args);
}

void IOSTranslateInternalsHandler::RegisterMessages() {
  web::BrowserState* browser_state = web_ui()->GetWebState()->GetBrowserState();
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(browser_state)->GetOriginalProfile();

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);

  const BrowserList::BrowserType browser_types =
      profile->IsOffTheRecord() ? BrowserList::BrowserType::kIncognito
                                : BrowserList::BrowserType::kRegularAndInactive;
  std::set<Browser*> browsers = browser_list->BrowsersOfType(browser_types);

  for (Browser* browser : browsers) {
    WebStateList* web_state_list = browser->GetWebStateList();
    for (int i = 0; i < web_state_list->count(); i++) {
      AddLanguageDetectionObserverForWebState(web_state_list->GetWebStateAt(i));
    }
  }

  AllWebStateListObservationRegistrar::Mode mode =
      profile->IsOffTheRecord()
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

#pragma mark - WebStateListObserver

void IOSTranslateInternalsHandler::Observer::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      handler_->RemoveLanguageDetectionObserverForWebState(
          detach_change.detached_web_state());
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      handler_->RemoveLanguageDetectionObserverForWebState(
          replace_change.replaced_web_state());
      handler_->AddLanguageDetectionObserverForWebState(
          replace_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      handler_->AddLanguageDetectionObserverForWebState(
          insert_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
  }
}
