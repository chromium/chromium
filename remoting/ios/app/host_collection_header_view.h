// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_HOST_COLLECTION_HEADER_VIEW_H_
#define REMOTING_IOS_APP_HOST_COLLECTION_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

// A simple view that displays a white text label on the collection view.
@interface HostCollectionHeaderView : UICollectionReusableView
@property(nonatomic) NSString* text;
@end

#endif  // REMOTING_IOS_APP_HOST_COLLECTION_HEADER_VIEW_H_
