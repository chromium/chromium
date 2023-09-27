// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/parcel_tracking/infobar_parcel_tracking_table_view_controller.h"

#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/parcel_tracking/infobar_parcel_tracking_modal_delegate.h"
#import "ios/web/public/annotations/custom_text_checking_result.h"

@implementation InfobarParcelTrackingTableViewController {
  // List of parcels.
  NSArray<CustomTextCheckingResult*>* parcelList_;
  // Indicates whether the parcels in `parcelList_` are being tracked.
  bool trackingParcels_;
  // Delegate for this view controller.
  id<InfobarModalDelegate, InfobarParcelTrackingModalDelegate> delegate_;
}

- (instancetype)initWithDelegate:
    (id<InfobarModalDelegate, InfobarParcelTrackingModalDelegate>)delegate {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    delegate_ = delegate;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  // TODO(crbug.com/1473449): implement.
}

#pragma mark - InfobarParcelTrackingModalConsumer

- (void)setParcelList:(NSArray<CustomTextCheckingResult*>*)parcels
    withTrackingStatus:(bool)tracking {
  parcelList_ = parcels;
  trackingParcels_ = tracking;
}

@end
