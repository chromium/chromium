// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/strike_database_factory.h"

#import "components/strike_database/strike_database.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace autofill {

// static
strike_database::StrikeDatabase* StrikeDatabaseFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<strike_database::StrikeDatabase>(
      profile,
      /*create=*/true);
}

// static
StrikeDatabaseFactory* StrikeDatabaseFactory::GetInstance() {
  static base::NoDestructor<StrikeDatabaseFactory> instance;
  return instance.get();
}

StrikeDatabaseFactory::StrikeDatabaseFactory()
    : ProfileKeyedServiceFactoryIOS("AutofillStrikeDatabase") {}

StrikeDatabaseFactory::~StrikeDatabaseFactory() = default;

std::unique_ptr<KeyedService> StrikeDatabaseFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  leveldb_proto::ProtoDatabaseProvider* db_provider =
      profile->GetProtoDatabaseProvider();

  return std::make_unique<strike_database::StrikeDatabase>(
      db_provider, profile->GetStatePath());
}

}  // namespace autofill
