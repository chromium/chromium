// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_START_SUGGEST_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_START_SUGGEST_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "components/search/start_suggest_service.h"

class ChromeBrowserState;

class StartSuggestServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static StartSuggestService* GetForBrowserState(
      ChromeBrowserState* browser_state,
      bool create_if_necessary);
  static StartSuggestServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<StartSuggestServiceFactory>;

  StartSuggestServiceFactory();
  ~StartSuggestServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  StartSuggestServiceFactory(const StartSuggestServiceFactory&) = delete;
  StartSuggestServiceFactory& operator=(const StartSuggestServiceFactory&) =
      delete;
};

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_START_SUGGEST_SERVICE_FACTORY_H_
