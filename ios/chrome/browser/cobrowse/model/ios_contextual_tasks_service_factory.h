// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_MODEL_IOS_CONTEXTUAL_TASKS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_COBROWSE_MODEL_IOS_CONTEXTUAL_TASKS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace contextual_tasks {
class ContextualTasksService;
}  // namespace contextual_tasks

// Factory creating ContextualTasksService and associating them to ProfileIOS.
class IOSContextualTasksServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static contextual_tasks::ContextualTasksService* GetForProfile(
      ProfileIOS* profile);

  static IOSContextualTasksServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSContextualTasksServiceFactory>;

  IOSContextualTasksServiceFactory();
  ~IOSContextualTasksServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_COBROWSE_MODEL_IOS_CONTEXTUAL_TASKS_SERVICE_FACTORY_H_
