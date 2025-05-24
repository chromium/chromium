// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/scoped_profile_keep_alive_ios.h"

#include <algorithm>

ScopedProfileKeepAliveIOS::ScopedProfileKeepAliveIOS() = default;

ScopedProfileKeepAliveIOS::ScopedProfileKeepAliveIOS(PassKey,
                                                     ProfileIOS* profile,
                                                     Cleanup cleanup)
    : profile_(profile), cleanup_(std::move(cleanup)) {}

ScopedProfileKeepAliveIOS::ScopedProfileKeepAliveIOS(
    ScopedProfileKeepAliveIOS&& other) = default;

ScopedProfileKeepAliveIOS& ScopedProfileKeepAliveIOS::operator=(
    ScopedProfileKeepAliveIOS&& other) {
  using std::swap;
  swap(cleanup_, other.cleanup_);
  swap(profile_, other.profile_);
  return *this;
}

ScopedProfileKeepAliveIOS::~ScopedProfileKeepAliveIOS() {
  Reset();
}

void ScopedProfileKeepAliveIOS::Reset() {
  // Ensure that profile() will return null before the profile is unloaded
  // to avoid having a dangling raw_ptr<...>.
  profile_ = nullptr;
  if (cleanup_) {
    std::move(cleanup_).Run();
  }
}
