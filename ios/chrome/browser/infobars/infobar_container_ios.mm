// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_container_ios.h"

#include <stddef.h>

#include "base/logging.h"
#include "ios/chrome/browser/infobars/infobar.h"
#import "ios/chrome/browser/ui/infobars/infobar_container_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

InfoBarContainerIOS::InfoBarContainerIOS(
    infobars::InfoBarContainer::Delegate* delegate,
    id<InfobarContainerConsumer> consumer)
    : InfoBarContainer(delegate), delegate_(delegate), consumer_(consumer) {
  DCHECK(delegate);
}

InfoBarContainerIOS::~InfoBarContainerIOS() {
  delegate_ = nullptr;
  RemoveAllInfoBarsForDestruction();
}

void InfoBarContainerIOS::PlatformSpecificAddInfoBar(infobars::InfoBar* infobar,
                                                     size_t position) {
  InfoBarIOS* infobar_ios = static_cast<InfoBarIOS*>(infobar);
  [consumer_ addInfoBar:infobar_ios position:position];
}

void InfoBarContainerIOS::PlatformSpecificRemoveInfoBar(
    infobars::InfoBar* infobar) {
  InfoBarIOS* infobar_ios = static_cast<InfoBarIOS*>(infobar);
  infobar_ios->RemoveView();
  // If computed_height() is 0, then the infobar was removed after an animation.
  // In this case, signal the delegate that the state changed.
  // Otherwise, the infobar is being replaced by another one. Do not call the
  // delegate in this case, as the delegate will be updated when the new infobar
  // is added.
  if (infobar->computed_height() == 0 && delegate_)
    delegate_->InfoBarContainerStateChanged(false);
}

void InfoBarContainerIOS::PlatformSpecificInfoBarStateChanged(
    bool is_animating) {
  [consumer_ setUserInteractionEnabled:!is_animating];
}
