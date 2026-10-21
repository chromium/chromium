// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_REMOTE_SUGGESTIONS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_REMOTE_SUGGESTIONS_SERVICE_FACTORY_H_

#import "components/omnibox/browser/remote_suggestions_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all RemoteSuggestionsServices and associates them with
// ProfileIOS.
class RemoteSuggestionsServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<RemoteSuggestionsServiceFactory,
                                                RemoteSuggestionsService> {
 public:
  explicit RemoteSuggestionsServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_REMOTE_SUGGESTIONS_SERVICE_FACTORY_H_
