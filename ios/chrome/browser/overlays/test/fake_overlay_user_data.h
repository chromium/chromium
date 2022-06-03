// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_TEST_FAKE_OVERLAY_USER_DATA_H_
#define IOS_CHROME_BROWSER_OVERLAYS_TEST_FAKE_OVERLAY_USER_DATA_H_

#include "ios/chrome/browser/overlays/public/overlay_user_data.h"

// Test OverlayUserData that can be used to store arbitrary pointers in
// OverlayRequests and OverlayResponses.
class FakeOverlayUserData : public OverlayUserData<FakeOverlayUserData> {
 public:
  // Accessor for value pointer.
  void* value() const { return value_; }

 private:
  OVERLAY_USER_DATA_SETUP(FakeOverlayUserData);
  FakeOverlayUserData(void* value = nullptr);

  void* value_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_TEST_FAKE_OVERLAY_USER_DATA_H_
