// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_popup_view_base.h"

#import <string_view>

#import "base/callback_list.h"
#import "base/functional/callback_forward.h"
#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"

OmniboxPopupViewBase::OmniboxPopupViewBase(OmniboxControllerIOS* controller)
    : controller_(controller) {}

OmniboxPopupViewBase::~OmniboxPopupViewBase() = default;

OmniboxEditModelIOS* OmniboxPopupViewBase::model() {
  return const_cast<OmniboxEditModelIOS*>(
      const_cast<const OmniboxPopupViewBase*>(this)->model());
}

const OmniboxEditModelIOS* OmniboxPopupViewBase::model() const {
  return controller_->edit_model();
}

OmniboxControllerIOS* OmniboxPopupViewBase::controller() {
  return const_cast<OmniboxControllerIOS*>(
      const_cast<const OmniboxPopupViewBase*>(this)->controller());
}

const OmniboxControllerIOS* OmniboxPopupViewBase::controller() const {
  return controller_;
}

std::u16string_view OmniboxPopupViewBase::GetAccessibleButtonTextForResult(
    size_t line) const {
  return {};
}

base::CallbackListSubscription OmniboxPopupViewBase::AddOpenListener(
    base::RepeatingClosure callback) {
  return on_popup_callbacks_.Add(std::move(callback));
}

void OmniboxPopupViewBase::NotifyOpenListeners() {
  on_popup_callbacks_.Notify();
}
