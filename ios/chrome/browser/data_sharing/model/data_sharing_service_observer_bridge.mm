// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/data_sharing_service_observer_bridge.h"

DataSharingServiceObserverBridge::DataSharingServiceObserverBridge(
    id<DataSharingServiceObserverDelegate> delegate)
    : delegate_(delegate) {}

DataSharingServiceObserverBridge::~DataSharingServiceObserverBridge() {}

void DataSharingServiceObserverBridge::OnGroupDataModelLoaded() {
  if ([delegate_ respondsToSelector:@selector(dataSharingServiceInitialized)]) {
    [delegate_ dataSharingServiceInitialized];
  }
}

void DataSharingServiceObserverBridge::OnGroupChanged(
    const data_sharing::GroupData& group_data,
    const base::Time& event_time) {
  if ([delegate_ respondsToSelector:@selector
                 (dataSharingServiceDidChangeGroup:atTime:)]) {
    [delegate_ dataSharingServiceDidChangeGroup:group_data atTime:event_time];
  }
}

void DataSharingServiceObserverBridge::OnGroupAdded(
    const data_sharing::GroupData& group_data,
    const base::Time& event_time) {
  if ([delegate_ respondsToSelector:@selector
                 (dataSharingServiceDidAddGroup:atTime:)]) {
    [delegate_ dataSharingServiceDidAddGroup:group_data atTime:event_time];
  }
}

void DataSharingServiceObserverBridge::OnGroupRemoved(
    const data_sharing::GroupId& group_id,
    const base::Time& event_time) {
  if ([delegate_ respondsToSelector:@selector
                 (dataSharingServiceDidRemoveGroup:atTime:)]) {
    [delegate_ dataSharingServiceDidRemoveGroup:group_id atTime:event_time];
  }
}

void DataSharingServiceObserverBridge::OnGroupMemberAdded(
    const data_sharing::GroupId& group_id,
    const GaiaId& member_gaia_id,
    const base::Time& event_time) {
  if ([delegate_ respondsToSelector:@selector
                 (dataSharingServiceDidAddMember:toGroup:atTime:)]) {
    [delegate_ dataSharingServiceDidAddMember:member_gaia_id
                                      toGroup:group_id
                                       atTime:event_time];
  }
}

void DataSharingServiceObserverBridge::OnGroupMemberRemoved(
    const data_sharing::GroupId& group_id,
    const GaiaId& member_gaia_id,
    const base::Time& event_time) {
  if ([delegate_ respondsToSelector:@selector
                 (dataSharingServiceDidRemoveMember:toGroup:atTime:)]) {
    [delegate_ dataSharingServiceDidRemoveMember:member_gaia_id
                                         toGroup:group_id
                                          atTime:event_time];
  }
}
