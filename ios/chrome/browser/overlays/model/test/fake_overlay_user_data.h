// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_TEST_FAKE_OVERLAY_USER_DATA_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_TEST_FAKE_OVERLAY_USER_DATA_H_

#include "ios/chrome/browser/overlays/model/public/overlay_user_data.h"

#import "base/memory/raw_ptr.h"

// Test OverlayUserData that can be used to store arbitrary pointers in
// OverlayRequests and OverlayResponses.
class FakeOverlayUserData : public OverlayUserData<FakeOverlayUserData> {
 public:
  // Accessor for value pointer.
  void* value() const { return value_; }

 private:
  OVERLAY_USER_DATA_SETUP(FakeOverlayUserData);
  FakeOverlayUserData(void* value = nullptr);

  raw_ptr<void> value_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_TEST_FAKE_OVERLAY_USER_DATA_H_
