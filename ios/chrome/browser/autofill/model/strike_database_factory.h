// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_STRIKE_DATABASE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_STRIKE_DATABASE_FACTORY_H_

#import <memory>

#import "components/strike_database/strike_database.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace autofill {

// Singleton that owns all StrikeDatabases and associates them with
// ProfileIOS.
class StrikeDatabaseFactory final : public TypedProfileKeyedServiceFactoryIOS<
                                        StrikeDatabaseFactory,
                                        strike_database::StrikeDatabase> {
 public:
  explicit StrikeDatabaseFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_STRIKE_DATABASE_FACTORY_H_
