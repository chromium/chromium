// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_MESSAGING_MESSAGING_BACKEND_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_MESSAGING_MESSAGING_BACKEND_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace tab_groups::messaging {

class MessagingBackendService;

// Factory for the messaging backend service.
class MessagingBackendServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static MessagingBackendService* GetForProfile(ProfileIOS* profile);
  static MessagingBackendServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<MessagingBackendServiceFactory>;

  MessagingBackendServiceFactory();
  ~MessagingBackendServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace tab_groups::messaging

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_MESSAGING_MESSAGING_BACKEND_SERVICE_FACTORY_H_
