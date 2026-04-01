// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/rules_service_observer_bridge.h"

namespace data_controls {

RulesServiceObserverBridge::RulesServiceObserverBridge(
    id<RulesServiceObserving> observer)
    : observer_(observer) {}

RulesServiceObserverBridge::~RulesServiceObserverBridge() = default;

void RulesServiceObserverBridge::OnRulesUpdated() {
  [observer_ onRulesUpdated];
}

}  // namespace data_controls
