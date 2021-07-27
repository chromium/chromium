// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_ios.h"

#include "base/check.h"
#include "ios/chrome/browser/infobars/infobar_controller.h"
#include "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/infobars/infobar_ui_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;
using infobars::InfoBarDelegate;

InfoBarIOS::InfoBarIOS(InfobarType infobar_type,
                       std::unique_ptr<InfoBarDelegate> delegate,
                       bool skip_banner)
    : InfoBar(std::move(delegate)),
      infobar_type_(infobar_type),
      skip_banner_(skip_banner) {}

InfoBarIOS::~InfoBarIOS() {
  ui_delegate_.delegate = nullptr;
  [ui_delegate_ detachView];
  ui_delegate_ = nil;
  for (auto& observer : observers_) {
    observer.InfobarDestroyed(this);
  }
}

void InfoBarIOS::set_accepted(bool accepted) {
  if (accepted_ == accepted)
    return;
  accepted_ = accepted;
  for (auto& observer : observers_) {
    observer.DidUpdateAcceptedState(this);
  }
}

void InfoBarIOS::set_high_priority(bool high_priority) {
  if (high_priority_ == high_priority)
    return;
  high_priority_ = high_priority;
}

id<InfobarUIDelegate> InfoBarIOS::InfobarUIDelegate() {
  DCHECK(ui_delegate_);
  return ui_delegate_;
}

void InfoBarIOS::RemoveView() {
  DCHECK(ui_delegate_);
  [ui_delegate_ removeView];
}

base::WeakPtr<InfoBarIOS> InfoBarIOS::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void InfoBarIOS::PlatformSpecificSetOwner() {
  if (!owner()) {
    ui_delegate_.delegate = nullptr;
  }
}

void InfoBarIOS::PlatformSpecificOnCloseSoon() {
  ui_delegate_.delegate = nullptr;
}

#pragma mark - InfoBarControllerDelegate

bool InfoBarIOS::IsOwned() {
  return owner() != nullptr;
}

void InfoBarIOS::RemoveInfoBar() {
  RemoveSelf();
}
