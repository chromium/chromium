// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/consent_auditor/model/consent_auditor_test_utils.h"

#include "components/consent_auditor/fake_consent_auditor.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

std::unique_ptr<KeyedService> BuildFakeConsentAuditor(ProfileIOS* profile) {
  return std::make_unique<consent_auditor::FakeConsentAuditor>();
}
