// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_REMOTE_SUGGESTIONS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_REMOTE_SUGGESTIONS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class RemoteSuggestionsService;

// Singleton that owns all RemoteSuggestionsServices and associates them with
// ProfileIOS.
class RemoteSuggestionsServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static RemoteSuggestionsService* GetForProfile(ProfileIOS* profile,
                                                 bool create_if_necessary);
  static RemoteSuggestionsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<RemoteSuggestionsServiceFactory>;

  RemoteSuggestionsServiceFactory();
  ~RemoteSuggestionsServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  RemoteSuggestionsServiceFactory(const RemoteSuggestionsServiceFactory&) =
      delete;
  RemoteSuggestionsServiceFactory& operator=(
      const RemoteSuggestionsServiceFactory&) = delete;
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_REMOTE_SUGGESTIONS_SERVICE_FACTORY_H_
