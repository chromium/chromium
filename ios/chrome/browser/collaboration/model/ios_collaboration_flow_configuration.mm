// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/ios_collaboration_flow_configuration.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

namespace collaboration {

#pragma mark - CollaborationFlowConfigurationShare

CollaborationFlowConfiguration::Type CollaborationFlowConfigurationShare::type()
    const {
  return kType;
}

CollaborationFlowConfigurationShare::CollaborationFlowConfigurationShare(
    ShareKitService* share_kit_service,
    base::WeakPtr<const TabGroup> tab_group,
    CommandDispatcher* command_dispatcher,
    UIViewController* base_view_controller)
    : share_kit_service_(share_kit_service),
      tab_group_(tab_group),
      command_dispatcher_(command_dispatcher),
      base_view_controller_(base_view_controller) {
  CHECK(share_kit_service_);
  CHECK(command_dispatcher_);
  CHECK(tab_group_);
  CHECK(base_view_controller_);
}

raw_ptr<ShareKitService>
CollaborationFlowConfigurationShare::share_kit_service() const {
  return share_kit_service_;
}

CommandDispatcher* CollaborationFlowConfigurationShare::command_dispatcher()
    const {
  return command_dispatcher_;
}

UIViewController* CollaborationFlowConfigurationShare::base_view_controller()
    const {
  return base_view_controller_;
}

CollaborationFlowConfigurationShare::~CollaborationFlowConfigurationShare() {}

#pragma mark - CollaborationFlowConfigurationJoin

CollaborationFlowConfiguration::Type CollaborationFlowConfigurationJoin::type()
    const {
  return kType;
}

CollaborationFlowConfigurationJoin::CollaborationFlowConfigurationJoin(
    ShareKitService* share_kit_service,
    const GURL& url,
    CommandDispatcher* command_dispatcher,
    UIViewController* base_view_controller)
    : share_kit_service_(share_kit_service),
      url_(url),
      command_dispatcher_(command_dispatcher),
      base_view_controller_(base_view_controller) {}

CommandDispatcher* CollaborationFlowConfigurationJoin::command_dispatcher()
    const {
  return command_dispatcher_;
}

raw_ptr<ShareKitService> CollaborationFlowConfigurationJoin::share_kit_service()
    const {
  return share_kit_service_;
}

UIViewController* CollaborationFlowConfigurationJoin::base_view_controller()
    const {
  return base_view_controller_;
}

CollaborationFlowConfigurationJoin::~CollaborationFlowConfigurationJoin() {}

}  // namespace collaboration
