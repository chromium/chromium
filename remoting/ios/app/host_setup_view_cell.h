// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_HOST_SETUP_VIEW_CELL_H_
#define REMOTING_IOS_APP_HOST_SETUP_VIEW_CELL_H_

#import <UIKit/UIKit.h>

// The collection cell for each step when setting up the host.
@interface HostSetupViewCell : UITableViewCell

- (void)setContentText:(NSString*)text number:(NSInteger)number;

@end

#endif  // REMOTING_IOS_APP_HOST_SETUP_VIEW_CELL_H_
