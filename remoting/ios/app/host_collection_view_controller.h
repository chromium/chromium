// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_HOST_COLLECTION_VIEW_CONTROLLER_H_
#define REMOTING_IOS_APP_HOST_COLLECTION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import <MaterialComponents/MaterialCollections.h>

#import "remoting/ios/app/host_collection_view_cell.h"
#import "remoting/ios/domain/host_info.h"

// The host collection view controller delegate provides the data for available
// hosts and receives selection events from the collection view controller.
@protocol HostCollectionViewControllerDelegate<NSObject>

// Notifies the delegate if a selection happens for the provided cell.
// The delegate should run the completionBlock when processing for this event
// has finished.
@optional
- (void)didSelectCell:(HostCollectionViewCell*)cell
           completion:(void (^)())completionBlock;

// The delegate should provide the HostInfo object for the given path if
// available from the cache.
- (HostInfo*)getHostAtIndexPath:(NSIndexPath*)path;

// The delegate must provide the total number of hosts currently cached.
- (NSInteger)getHostCount;

@end

@interface HostCollectionViewController : MDCCollectionViewController

@property(weak, nonatomic) id<HostCollectionViewControllerDelegate> delegate;
@property(weak, nonatomic) id<UIScrollViewDelegate> scrollViewDelegate;

@end

#endif  // REMOTING_IOS_APP_HOST_COLLECTION_VIEW_CONTROLLER_H_
