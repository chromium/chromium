// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/ios_collaboration_flow_configuration.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"

namespace collaboration {

#pragma mark - CollaborationFlowConfiguration

CollaborationFlowConfiguration::CollaborationFlowConfiguration(
    ShareKitService* share_kit_service,
    Browser* browser,
    UIViewController* base_view_controller)
    : share_kit_service_(share_kit_service),
      browser_(browser),
      base_view_controller_(base_view_controller) {
  CHECK(share_kit_service_);
  CHECK(browser_);
  CHECK(base_view_controller_);
}

#pragma mark - CollaborationFlowConfigurationShare

CollaborationFlowConfiguration::Type CollaborationFlowConfigurationShare::type()
    const {
  return kType;
}

CollaborationFlowConfigurationShare::CollaborationFlowConfigurationShare(
    ShareKitService* share_kit_service,
    Browser* browser,
    UIViewController* base_view_controller,
    base::WeakPtr<const TabGroup> tab_group)
    : CollaborationFlowConfiguration(share_kit_service,
                                     browser,
                                     base_view_controller),
      tab_group_(tab_group) {
  CHECK(tab_group_);
}

CollaborationFlowConfigurationShare::~CollaborationFlowConfigurationShare() {}

#pragma mark - CollaborationFlowConfigurationJoin

CollaborationFlowConfiguration::Type CollaborationFlowConfigurationJoin::type()
    const {
  return kType;
}

CollaborationFlowConfigurationJoin::CollaborationFlowConfigurationJoin(
    ShareKitService* share_kit_service,
    Browser* browser,
    UIViewController* base_view_controller,
    const GURL& url)
    : CollaborationFlowConfiguration(share_kit_service,
                                     browser,
                                     base_view_controller),
      url_(url) {}

CollaborationFlowConfigurationJoin::~CollaborationFlowConfigurationJoin() {}

}  // namespace collaboration
