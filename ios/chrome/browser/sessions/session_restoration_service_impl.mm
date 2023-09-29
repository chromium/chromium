// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_restoration_service_impl.h"

SessionRestorationServiceImpl::SessionRestorationServiceImpl() = default;

SessionRestorationServiceImpl::~SessionRestorationServiceImpl() = default;

void SessionRestorationServiceImpl::Shutdown() {}

void SessionRestorationServiceImpl::AddObserver(
    SessionRestorationObserver* observer) {
  observers_.AddObserver(observer);
}

void SessionRestorationServiceImpl::RemoveObserver(
    SessionRestorationObserver* observer) {
  observers_.RemoveObserver(observer);
}
