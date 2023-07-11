// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeSafetyCheckManager::IOSChromeSafetyCheckManager() {}

IOSChromeSafetyCheckManager::~IOSChromeSafetyCheckManager() {
  DCHECK(observers_.empty());
}

void IOSChromeSafetyCheckManager::Shutdown() {
  for (auto& observer : observers_) {
    observer.ManagerWillShutdown(this);
  }

  DCHECK(observers_.empty());
}

void IOSChromeSafetyCheckManager::AddObserver(
    IOSChromeSafetyCheckManagerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.AddObserver(observer);
}

void IOSChromeSafetyCheckManager::RemoveObserver(
    IOSChromeSafetyCheckManagerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.RemoveObserver(observer);
}
