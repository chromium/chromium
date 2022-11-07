// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FakeAuthenticationServiceDelegate::FakeAuthenticationServiceDelegate() =
    default;

FakeAuthenticationServiceDelegate::~FakeAuthenticationServiceDelegate() =
    default;

void FakeAuthenticationServiceDelegate::ClearBrowsingData(
    ProceduralBlock completion) {
  ++clear_browsing_data_counter_;
  if (completion) {
    completion();
  }
}
