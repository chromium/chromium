// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_ENTITY_DATA_MANAGER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_ENTITY_DATA_MANAGER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/scoped_observation.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"

// Objective-C delegate for EntityDataManager notifications.
@protocol IOSAutofillEntityDataManagerObserver <NSObject>

// Called when the entity instances in EntityDataManager changed.
- (void)onEntityInstancesChanged;

@end

namespace autofill {

// Bridge class to forward EntityDataManager::Observer notifications to an
// Objective-C delegate.
class IOSAutofillEntityDataManagerObserverBridge
    : public EntityDataManager::Observer {
 public:
  IOSAutofillEntityDataManagerObserverBridge(
      EntityDataManager* entity_data_manager,
      id<IOSAutofillEntityDataManagerObserver> delegate);

  IOSAutofillEntityDataManagerObserverBridge(
      const IOSAutofillEntityDataManagerObserverBridge&) = delete;
  IOSAutofillEntityDataManagerObserverBridge& operator=(
      const IOSAutofillEntityDataManagerObserverBridge&) = delete;

  ~IOSAutofillEntityDataManagerObserverBridge() override;

  // EntityDataManager::Observer implementation.
  void OnEntityInstancesChanged() override;

 private:
  __weak id<IOSAutofillEntityDataManagerObserver> delegate_;
  base::ScopedObservation<EntityDataManager, EntityDataManager::Observer>
      scoped_observation_{this};
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_ENTITY_DATA_MANAGER_OBSERVER_BRIDGE_H_
