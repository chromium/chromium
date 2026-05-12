// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_service.h"

#import "components/prefs/pref_service.h"

LevelUpService::LevelUpService() {}

LevelUpService::~LevelUpService() = default;

bool LevelUpService::IsOptedIn() const {
  // TODO: Implement.
  return false;
}

void LevelUpService::SetOptIn(bool opted_in) {
  // TODO: Implement.
}

int LevelUpService::GetCurrentLevel() const {
  // TODO: Implement.
  return 0;
}

void LevelUpService::Shutdown() {
  // TODO: Implement.
}
