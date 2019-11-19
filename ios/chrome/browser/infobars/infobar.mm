// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar.h"

#include "base/logging.h"
#include "ios/chrome/browser/infobars/infobar_controller.h"
#import "ios/chrome/browser/ui/infobars/infobar_ui_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;
using infobars::InfoBarDelegate;

InfoBarIOS::InfoBarIOS(id<InfobarUIDelegate> controller,
                       std::unique_ptr<InfoBarDelegate> delegate)
    : InfoBar(std::move(delegate)), controller_(controller) {
  DCHECK(controller_);
  [controller_ setDelegate:this];
}

InfoBarIOS::~InfoBarIOS() {
  DCHECK(controller_);
  [controller_ detachView];
  controller_ = nil;
}

id<InfobarUIDelegate> InfoBarIOS::InfobarUIDelegate() {
  DCHECK(controller_);
  return controller_;
}

void InfoBarIOS::RemoveView() {
  DCHECK(controller_);
  [controller_ removeView];
}

#pragma mark - InfoBarControllerDelegate

bool InfoBarIOS::IsOwned() {
  return owner() != nullptr;
}

void InfoBarIOS::RemoveInfoBar() {
  RemoveSelf();
}
