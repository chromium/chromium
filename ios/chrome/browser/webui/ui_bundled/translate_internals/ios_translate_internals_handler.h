// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_TRANSLATE_INTERNALS_IOS_TRANSLATE_INTERNALS_HANDLER_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_TRANSLATE_INTERNALS_IOS_TRANSLATE_INTERNALS_HANDLER_H_

#include <string>
#include <string_view>

#import "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "components/language/ios/browser/ios_language_detection_tab_helper.h"
#include "components/translate/translate_internals/translate_internals_handler.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

namespace web {
class WebState;
}  // namespace web

class AllWebStateListObservationRegistrar;

// The handler for JavaScript messages for chrome://translate-internals.
class IOSTranslateInternalsHandler
    : public translate::TranslateInternalsHandler,
      public web::WebUIIOSMessageHandler,

      public language::IOSLanguageDetectionTabHelper::Observer {
 public:
  IOSTranslateInternalsHandler();
  // Not copyable or assignable.
  IOSTranslateInternalsHandler(const IOSTranslateInternalsHandler&) = delete;
  IOSTranslateInternalsHandler& operator=(const IOSTranslateInternalsHandler&) =
      delete;
  ~IOSTranslateInternalsHandler() override;

  // translate::TranslateInternalsHandler.
  translate::TranslateClient* GetTranslateClient() override;
  variations::VariationsService* GetVariationsService() override;
  void RegisterMessageCallback(std::string_view message,
                               MessageCallback callback) override;
  void CallJavascriptFunction(std::string_view function_name,
                              base::span<const base::ValueView> args) override;

  // web::WebUIIOSMessageHandler.
  void RegisterMessages() override;

  // language::IOSLanguageDetectionTabHelper::Observer
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;
  void IOSLanguageDetectionTabHelperWasDestroyed(
      language::IOSLanguageDetectionTabHelper* tab_helper) override;

  // Adds this instance as an observer of the IOSLanguageDetectionTabHelper
  // associated with `web_state`.
  void AddLanguageDetectionObserverForWebState(web::WebState* web_state);
  // Removes this instance as an observer of the IOSLanguageDetectionTabHelper
  // associated with `web_state`.
  void RemoveLanguageDetectionObserverForWebState(web::WebState* web_state);

 private:
  // Inner observer class, owned by the `registrar_`.
  class Observer : public WebStateListObserver {
   public:
    explicit Observer(IOSTranslateInternalsHandler* handler);
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override;

   private:
    // WebStateListObserver:
    void WebStateListDidChange(WebStateList* web_state_list,
                               const WebStateListChange& change,
                               const WebStateListStatus& status) override;

    raw_ptr<IOSTranslateInternalsHandler> handler_;
  };

  std::unique_ptr<AllWebStateListObservationRegistrar> registrar_;
  base::ScopedMultiSourceObservation<
      language::IOSLanguageDetectionTabHelper,
      language::IOSLanguageDetectionTabHelper::Observer>
      scoped_tab_helper_observations_{this};
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_TRANSLATE_INTERNALS_IOS_TRANSLATE_INTERNALS_HANDLER_H_
