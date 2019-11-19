// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_container_ios.h"

#include "base/metrics/histogram_macros.h"
#include "ios/chrome/browser/infobars/infobar.h"
#import "ios/chrome/browser/ui/infobars/infobar_container_consumer.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/infobar_ui_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

InfoBarContainerIOS::InfoBarContainerIOS(
    id<InfobarContainerConsumer> consumer,
    id<InfobarContainerConsumer> legacyConsumer)
    : InfoBarContainer(nullptr),
      consumer_(consumer),
      legacyConsumer_(legacyConsumer) {}

InfoBarContainerIOS::~InfoBarContainerIOS() {
  RemoveAllInfoBarsForDestruction();
}

void InfoBarContainerIOS::ChangeInfoBarManager(
    infobars::InfoBarManager* infobar_manager) {
  [consumer_ infobarManagerWillChange];
  InfoBarContainer::ChangeInfoBarManager(infobar_manager);
  info_bar_manager_ = infobar_manager;
}

void InfoBarContainerIOS::PlatformSpecificAddInfoBar(infobars::InfoBar* infobar,
                                                     size_t position) {
  InfoBarIOS* infobar_ios = static_cast<InfoBarIOS*>(infobar);
  id<InfobarUIDelegate> delegate = infobar_ios->InfobarUIDelegate();

  // Record the number of multiple Infobars being presented at the same time.
  // This doesn't differentiate between "Messages" or legacy Infobars.
  if (info_bar_manager_ && info_bar_manager_->infobar_count() > 0) {
    int kMaxValue = 10;
    UMA_HISTOGRAM_EXACT_LINEAR("Mobile.Messages.ConcurrentPresented",
                               info_bar_manager_->infobar_count(), kMaxValue);
  }

  if ([delegate isPresented]) {
    // Only InfobarUIReboot Infobars should be presented using the non legacy
    // consumer.
    DCHECK(IsInfobarUIRebootEnabled());
    [consumer_ addInfoBarWithDelegate:delegate];
  } else {
    [legacyConsumer_ addInfoBarWithDelegate:delegate];
  }
}

void InfoBarContainerIOS::PlatformSpecificRemoveInfoBar(
    infobars::InfoBar* infobar) {
  InfoBarIOS* infobar_ios = static_cast<InfoBarIOS*>(infobar);
  infobar_ios->RemoveView();
}

void InfoBarContainerIOS::PlatformSpecificInfoBarStateChanged(
    bool is_animating) {
  [consumer_ setUserInteractionEnabled:!is_animating];
  [legacyConsumer_ setUserInteractionEnabled:!is_animating];
}
