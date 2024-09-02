// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_LIST_FAMILY_MEMBERS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_LIST_FAMILY_MEMBERS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/supervised_user/core/browser/list_family_members_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

// Singleton that owns ListFamilyMembersService objects and associates
// them with Profiles.
class ListFamilyMembersServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static supervised_user::ListFamilyMembersService* GetForProfile(
      ProfileIOS* profile);

  static ListFamilyMembersServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ListFamilyMembersServiceFactory>;

  ListFamilyMembersServiceFactory();
  ~ListFamilyMembersServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_LIST_FAMILY_MEMBERS_SERVICE_FACTORY_H_
