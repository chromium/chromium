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

#pragma mark - CollaborationFlowConfigurationShare

CollaborationFlowConfigurationShare::CollaborationFlowConfigurationShare(
    base::WeakPtr<const TabGroup> tab_group)
    : tab_group_(tab_group) {
  CHECK(tab_group_);
}

CollaborationFlowConfigurationShare::~CollaborationFlowConfigurationShare() {}

CollaborationFlowConfiguration::Type CollaborationFlowConfigurationShare::type()
    const {
  return kType;
}

#pragma mark - CollaborationFlowConfigurationJoin

CollaborationFlowConfigurationJoin::CollaborationFlowConfigurationJoin(
    const GURL& url)
    : url_(url) {}

CollaborationFlowConfigurationJoin::~CollaborationFlowConfigurationJoin() {}

CollaborationFlowConfiguration::Type CollaborationFlowConfigurationJoin::type()
    const {
  return kType;
}

}  // namespace collaboration
