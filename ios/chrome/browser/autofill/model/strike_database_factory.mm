// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/strike_database_factory.h"

#import "components/autofill/core/browser/strike_databases/strike_database.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace autofill {

// static
StrikeDatabase* StrikeDatabaseFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<StrikeDatabase>(profile,
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
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  leveldb_proto::ProtoDatabaseProvider* db_provider =
      profile->GetProtoDatabaseProvider();

  return std::make_unique<autofill::StrikeDatabase>(db_provider,
                                                    profile->GetStatePath());
}

}  // namespace autofill
