// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_SNIPPETS_IOS_CHROME_CONTENT_SUGGESTIONS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_NTP_SNIPPETS_IOS_CHROME_CONTENT_SUGGESTIONS_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace ntp_snippets {
class ContentSuggestionsService;
}  // namespace ntp_snippets

// A factory to create a ContentSuggestionsService and associate it to
// ChromeBrowserState.
class IOSChromeContentSuggestionsServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static ntp_snippets::ContentSuggestionsService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static IOSChromeContentSuggestionsServiceFactory* GetInstance();

  // Returns the default factory used to build ContentSuggestionsServices. Can
  // be registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

  IOSChromeContentSuggestionsServiceFactory(
      const IOSChromeContentSuggestionsServiceFactory&) = delete;
  IOSChromeContentSuggestionsServiceFactory& operator=(
      const IOSChromeContentSuggestionsServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSChromeContentSuggestionsServiceFactory>;

  IOSChromeContentSuggestionsServiceFactory();
  ~IOSChromeContentSuggestionsServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_NTP_SNIPPETS_IOS_CHROME_CONTENT_SUGGESTIONS_SERVICE_FACTORY_H_
