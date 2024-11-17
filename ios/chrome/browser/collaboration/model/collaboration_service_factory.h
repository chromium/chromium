// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_COLLABORATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_COLLABORATION_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace collaboration {

class CollaborationService;

// Factory for CollaborationService.
class CollaborationServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static CollaborationService* GetForProfile(ProfileIOS* profile);
  static CollaborationServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<CollaborationServiceFactory>;

  CollaborationServiceFactory();
  ~CollaborationServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace collaboration

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_COLLABORATION_SERVICE_FACTORY_H_
