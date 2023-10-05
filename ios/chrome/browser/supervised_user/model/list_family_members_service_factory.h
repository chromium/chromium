// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_LIST_FAMILY_MEMBERS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_LIST_FAMILY_MEMBERS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/supervised_user/core/browser/list_family_members_service.h"

class ChromeBrowserState;

// Singleton that owns ListFamilyMembersService object and associates
// them with ChromeBrowserState.
class ListFamilyMembersServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static supervised_user::ListFamilyMembersService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static ListFamilyMembersServiceFactory* GetInstance();

  ListFamilyMembersServiceFactory(const ListFamilyMembersServiceFactory&) =
      delete;
  ListFamilyMembersServiceFactory& operator=(
      const ListFamilyMembersServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<ListFamilyMembersServiceFactory>;

  ListFamilyMembersServiceFactory();
  ~ListFamilyMembersServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_LIST_FAMILY_MEMBERS_SERVICE_FACTORY_H_
