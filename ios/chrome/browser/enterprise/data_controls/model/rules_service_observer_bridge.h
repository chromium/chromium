// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_RULES_SERVICE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_RULES_SERVICE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "components/enterprise/data_controls/core/browser/rules_service_base.h"

// Objective-C protocol for observing data_controls::RulesServiceBase changes.
@protocol RulesServiceObserving <NSObject>
- (void)onRulesUpdated;
@end

namespace data_controls {

// Bridge class that observes data_controls::RulesServiceBase on behalf of
// Objective-C clients.
class RulesServiceObserverBridge : public RulesServiceBase::Observer {
 public:
  explicit RulesServiceObserverBridge(id<RulesServiceObserving> observer);
  RulesServiceObserverBridge(const RulesServiceObserverBridge&) = delete;
  RulesServiceObserverBridge& operator=(const RulesServiceObserverBridge&) =
      delete;
  ~RulesServiceObserverBridge() override;

  // RulesServiceBase::Observer:
  void OnRulesUpdated() override;

 private:
  __weak id<RulesServiceObserving> observer_;
};

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_RULES_SERVICE_OBSERVER_BRIDGE_H_
