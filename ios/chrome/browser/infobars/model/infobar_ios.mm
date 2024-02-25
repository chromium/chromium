// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/infobar_ios.h"

#import "base/check.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"

using infobars::InfoBar;
using infobars::InfoBarDelegate;

InfoBarIOS::InfoBarIOS(InfobarType infobar_type,
                       std::unique_ptr<InfoBarDelegate> delegate,
                       bool skip_banner)
    : InfoBar(std::move(delegate)),
      infobar_type_(infobar_type),
      skip_banner_(skip_banner) {}

InfoBarIOS::~InfoBarIOS() {
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

base::WeakPtr<InfoBarIOS> InfoBarIOS::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

#pragma mark - InfoBarControllerDelegate

bool InfoBarIOS::IsOwned() {
  return owner() != nullptr;
}

void InfoBarIOS::RemoveInfoBar() {
  RemoveSelf();
}
