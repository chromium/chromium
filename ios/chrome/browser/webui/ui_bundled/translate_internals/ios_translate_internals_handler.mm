// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/translate_internals/ios_translate_internals_handler.h"

#import <string_view>

#import "base/notreached.h"
#import "base/scoped_observation.h"
#import "components/translate/core/common/language_detection_details.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_observer.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/translate/model/translate_service_ios.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/webui/web_ui_ios.h"

namespace {

// Returns whether the Browser is regular or inactive.
bool IsRegularOrInactiveBrowser(Browser* browser) {
  switch (browser->type()) {
    case Browser::Type::kRegular:
    case Browser::Type::kInactive:
      return true;

    case Browser::Type::kIncognito:
    case Browser::Type::kTemporary:
      return false;
  }
}

}  // namespace

#pragma mark - IOSTranslateInternalsHandler::Observer

class IOSTranslateInternalsHandler::Observer : public BrowserListObserver,
                                               public WebStateListObserver,
                                               public web::WebStateObserver {
 public:
  explicit Observer(IOSTranslateInternalsHandler* handler);
  ~Observer() override = default;

  void Start(BrowserList* browser_list);
  void Stop();

 private:
  // BrowserListObserver:
  void OnBrowserAdded(const BrowserList* browser_list,
                      Browser* browser) override;
  void OnBrowserRemoved(const BrowserList* browser_list,
                        Browser* browser) override;
  void OnBrowserListShutdown(BrowserList* browser_list) override;

  // WebStateListObserver:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // WebStateObserver:
  void WebStateRealized(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Helper functions to to observe the web state.
  // If `web_state` is realized, directly observe the language detection through
  // the handler. Otherwise observe the `web_state` until it is realized.
  // RemoveLanguageDetectionObserverForWebState removes the observers that were
  // added.
  void AddLanguageDetectionObserverForWebState(web::WebState* web_state);
  void RemoveLanguageDetectionObserverForWebState(web::WebState* web_state);

  raw_ptr<IOSTranslateInternalsHandler> handler_;

  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};

  base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>
      web_state_list_observations_{this};

  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      scoped_web_state_observations_{this};
};

IOSTranslateInternalsHandler::Observer::Observer(
    IOSTranslateInternalsHandler* handler)
    : handler_(handler) {
  DCHECK(handler_);
}

void IOSTranslateInternalsHandler::Observer::Start(BrowserList* browser_list) {
  browser_list_observation_.Observe(browser_list);

  // Only consider regular and inactive Browsers.
  const BrowserList::BrowserType browser_types =
      BrowserList::BrowserType::kRegularAndInactive;

  for (Browser* browser : browser_list->BrowsersOfType(browser_types)) {
    OnBrowserAdded(browser_list, browser);
  }
}

void IOSTranslateInternalsHandler::Observer::Stop() {
  browser_list_observation_.Reset();
  web_state_list_observations_.RemoveAllObservations();
  scoped_web_state_observations_.RemoveAllObservations();
  handler_->RemoveAllLanguageDetectionObservers();
}

#pragma mark - IOSTranslateInternalsHandler::Observer (BrowserListObserver)

void IOSTranslateInternalsHandler::Observer::OnBrowserAdded(
    const BrowserList* browser_list,
    Browser* browser) {
  if (!IsRegularOrInactiveBrowser(browser)) {
    return;
  }

  WebStateList* web_state_list = browser->GetWebStateList();
  web_state_list_observations_.AddObservation(web_state_list);

  const int web_state_list_count = web_state_list->count();
  for (int i = 0; i < web_state_list_count; i++) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    AddLanguageDetectionObserverForWebState(web_state);
  }
}

void IOSTranslateInternalsHandler::Observer::OnBrowserRemoved(
    const BrowserList* browser_list,
    Browser* browser) {
  if (!IsRegularOrInactiveBrowser(browser)) {
    return;
  }

  WebStateList* web_state_list = browser->GetWebStateList();
  web_state_list_observations_.RemoveObservation(web_state_list);

  const int web_state_list_count = web_state_list->count();
  for (int i = 0; i < web_state_list_count; i++) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    RemoveLanguageDetectionObserverForWebState(web_state);
  }
}

void IOSTranslateInternalsHandler::Observer::OnBrowserListShutdown(
    BrowserList* browser_list) {
  Stop();
}

#pragma mark - IOSTranslateInternalsHandler::Observer (WebStateListObserver)

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
      RemoveLanguageDetectionObserverForWebState(
          detach_change.detached_web_state());
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      RemoveLanguageDetectionObserverForWebState(
          replace_change.replaced_web_state());
      AddLanguageDetectionObserverForWebState(
          replace_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      AddLanguageDetectionObserverForWebState(
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

#pragma mark - IOSTranslateInternalsHandler::Observer (WebStateObserver)

void IOSTranslateInternalsHandler::Observer::WebStateRealized(
    web::WebState* web_state) {
  AddLanguageDetectionObserverForWebState(web_state);
  scoped_web_state_observations_.RemoveObservation(web_state);
}

void IOSTranslateInternalsHandler::Observer::WebStateDestroyed(
    web::WebState* web_state) {
  scoped_web_state_observations_.RemoveObservation(web_state);
}

#pragma mark - IOSTranslateInternalsHandler::Observer (Private)

void IOSTranslateInternalsHandler::Observer::
    AddLanguageDetectionObserverForWebState(web::WebState* web_state) {
  if (web_state->IsRealized()) {
    handler_->AddLanguageDetectionObserverForWebState(web_state);
  } else {
    scoped_web_state_observations_.AddObservation(web_state);
  }
}

void IOSTranslateInternalsHandler::Observer::
    RemoveLanguageDetectionObserverForWebState(web::WebState* web_state) {
  if (web_state->IsRealized()) {
    handler_->RemoveLanguageDetectionObserverForWebState(web_state);
  } else {
    scoped_web_state_observations_.RemoveObservation(web_state);
  }
}

#pragma mark - IOSTranslateInternalsHandler

IOSTranslateInternalsHandler::IOSTranslateInternalsHandler()
    : observer_(std::make_unique<Observer>(this)) {}

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
  observer_->Stop();
  observer_->Start(
      BrowserListFactory::GetForProfile(ProfileIOS::FromBrowserState(
          web_ui()->GetWebState()->GetBrowserState())));

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
  NOTREACHED();
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

void IOSTranslateInternalsHandler::RemoveAllLanguageDetectionObservers() {
  scoped_tab_helper_observations_.RemoveAllObservations();
}
