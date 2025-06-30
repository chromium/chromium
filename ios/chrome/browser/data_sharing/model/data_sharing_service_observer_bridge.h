// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_SERVICE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_SERVICE_OBSERVER_BRIDGE_H_

#import "components/data_sharing/public/data_sharing_service.h"

// Objective-C protocol equivalent of the
// data_sharing::DataSharingService::Observer C++ class. Those methods are
// called through the bridge. The method names are similar to the C++ ones.
@protocol DataSharingServiceObserverDelegate <NSObject>
@optional
- (void)dataSharingServiceInitialized;
- (void)dataSharingServiceDidChangeGroup:
            (const data_sharing::GroupData&)groupData
                                  atTime:(base::Time)eventTime;
- (void)dataSharingServiceDidAddGroup:(const data_sharing::GroupData&)groupData
                               atTime:(base::Time)eventTime;
- (void)dataSharingServiceDidRemoveGroup:(const data_sharing::GroupId&)groupId
                                  atTime:(base::Time)eventTime;
- (void)dataSharingServiceDidAddMember:(const GaiaId&)memberId
                               toGroup:(const data_sharing::GroupId&)groupId
                                atTime:(base::Time)eventTime;
- (void)dataSharingServiceDidRemoveMember:(const GaiaId&)memberId
                                  toGroup:(const data_sharing::GroupId&)groupId
                                   atTime:(base::Time)eventTime;
- (void)dataSharingServiceDestroyed;
@end

// Bridge class to forward events from the DataSharingService to Objective-C
// protocol DataSharingServiceObserverDelegate.
class DataSharingServiceObserverBridge final
    : public data_sharing::DataSharingService::Observer {
 public:
  explicit DataSharingServiceObserverBridge(
      id<DataSharingServiceObserverDelegate> delegate);

  DataSharingServiceObserverBridge(const DataSharingServiceObserverBridge&) =
      delete;
  DataSharingServiceObserverBridge& operator=(
      const DataSharingServiceObserverBridge&) = delete;

  ~DataSharingServiceObserverBridge() override;

  // DataSharingService::Observer implementation.
  void OnGroupDataModelLoaded() override;
  void OnGroupChanged(const data_sharing::GroupData& group_data,
                      const base::Time& event_time) override;
  void OnGroupAdded(const data_sharing::GroupData& group_data,
                    const base::Time& event_time) override;
  void OnGroupRemoved(const data_sharing::GroupId& group_id,
                      const base::Time& event_time) override;
  void OnGroupMemberAdded(const data_sharing::GroupId& group_id,
                          const GaiaId& member_gaia_id,
                          const base::Time& event_time) override;
  void OnGroupMemberRemoved(const data_sharing::GroupId& group_id,
                            const GaiaId& member_gaia_id,
                            const base::Time& event_time) override;
  void OnDataSharingServiceDestroyed() override;

 private:
  __weak id<DataSharingServiceObserverDelegate> delegate_ = nil;
};

#endif  // IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_SERVICE_OBSERVER_BRIDGE_H_
