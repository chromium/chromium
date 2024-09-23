// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"

FakeAuthenticationServiceDelegate::FakeAuthenticationServiceDelegate() =
    default;

FakeAuthenticationServiceDelegate::~FakeAuthenticationServiceDelegate() =
    default;

void FakeAuthenticationServiceDelegate::ClearBrowsingData(
    base::OnceClosure completion) {
  ++clear_browsing_data_counter_;
  if (completion) {
    std::move(completion).Run();
  }
}

void FakeAuthenticationServiceDelegate::ClearBrowsingDataForSignedinPeriod(
    base::OnceClosure completion) {
  ++clear_browsing_data_from_signin_counter_;
  if (completion) {
    std::move(completion).Run();
  }
}
