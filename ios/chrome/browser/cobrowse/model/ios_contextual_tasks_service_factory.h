// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_MODEL_IOS_CONTEXTUAL_TASKS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_COBROWSE_MODEL_IOS_CONTEXTUAL_TASKS_SERVICE_FACTORY_H_

#import "components/contextual_tasks/public/contextual_tasks_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Factory creating ContextualTasksService and associating them to ProfileIOS.
class IOSContextualTasksServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSContextualTasksServiceFactory,
          contextual_tasks::ContextualTasksService> {
 public:
  explicit IOSContextualTasksServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_COBROWSE_MODEL_IOS_CONTEXTUAL_TASKS_SERVICE_FACTORY_H_
