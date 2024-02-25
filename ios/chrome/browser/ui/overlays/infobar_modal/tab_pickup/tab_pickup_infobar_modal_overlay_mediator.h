// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_TAB_PICKUP_TAB_PICKUP_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_TAB_PICKUP_TAB_PICKUP_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/ui/infobars/modals/tab_pickup/infobar_tab_pickup_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_mediator.h"

@protocol InfobarTabPickupConsumer;
class PrefService;

namespace syncer {
class SyncService;
}  // namespace syncer

// Mediator that configures the modal UI for tab pickup infobar.
@interface TabPickupInfobarModalOverlayMediator
    : InfobarModalOverlayMediator <InfobarModalDelegate,
                                   InfobarTabPickupTableViewControllerDelegate>

// Designated initializer. All the parameters should not be null.
// `localPrefService`: preference service from the application context.
// `syncService` sync service.
// `consumer`: consumer that will be notified when the data change.
// `request`' : request's config.
- (instancetype)initWithUserLocalPrefService:(PrefService*)localPrefService
                                 syncService:(syncer::SyncService*)syncService
                                    consumer:
                                        (id<InfobarTabPickupConsumer>)consumer
                                     request:(OverlayRequest*)request
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithRequest:(OverlayRequest*)request NS_UNAVAILABLE;

// Consumer that is configured by this mediator.
@property(nonatomic, weak) id<InfobarTabPickupConsumer> consumer;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_TAB_PICKUP_TAB_PICKUP_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
