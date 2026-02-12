// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_observer_bridge.h"

namespace autofill {

IOSAutofillEntityDataManagerObserverBridge::
    IOSAutofillEntityDataManagerObserverBridge(
        EntityDataManager* entity_data_manager,
        id<IOSAutofillEntityDataManagerObserver> delegate)
    : delegate_(delegate) {
  scoped_observation_.Observe(entity_data_manager);
}

IOSAutofillEntityDataManagerObserverBridge::
    ~IOSAutofillEntityDataManagerObserverBridge() = default;

void IOSAutofillEntityDataManagerObserverBridge::OnEntityInstancesChanged() {
  [delegate_ onEntityInstancesChanged];
}

}  // namespace autofill
