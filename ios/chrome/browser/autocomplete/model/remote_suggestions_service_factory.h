// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_REMOTE_SUGGESTIONS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_REMOTE_SUGGESTIONS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class RemoteSuggestionsService;

// Singleton that owns all RemoteSuggestionsServices and associates them with
// ProfileIOS.
class RemoteSuggestionsServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static RemoteSuggestionsService* GetForProfile(ProfileIOS* profile,
                                                 bool create_if_necessary);
  static RemoteSuggestionsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<RemoteSuggestionsServiceFactory>;

  RemoteSuggestionsServiceFactory();
  ~RemoteSuggestionsServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_REMOTE_SUGGESTIONS_SERVICE_FACTORY_H_
