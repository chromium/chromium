// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_READING_LIST_READING_LIST_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_READING_LIST_READING_LIST_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/ui/infobars/modals/infobar_reading_list_modal_consumer.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_reading_list_modal_delegate.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_mediator.h"

// Mediator that configures the modal UI for a Reading List infobar.
@interface ReadingListInfobarModalOverlayMediator
    : InfobarModalOverlayMediator <InfobarReadingListModalDelegate>

// The consumer that is configured by this mediator.  Setting to a new value
// configures the new consumer.
@property(nonatomic, weak) id<InfobarReadingListModalConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_READING_LIST_READING_LIST_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
