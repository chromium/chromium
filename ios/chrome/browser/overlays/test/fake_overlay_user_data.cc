// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"

OVERLAY_USER_DATA_SETUP_IMPL(FakeOverlayUserData);

FakeOverlayUserData::FakeOverlayUserData(void* value) : value_(value) {}
