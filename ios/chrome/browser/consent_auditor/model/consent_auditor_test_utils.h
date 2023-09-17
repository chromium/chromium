// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONSENT_AUDITOR_MODEL_CONSENT_AUDITOR_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_CONSENT_AUDITOR_MODEL_CONSENT_AUDITOR_TEST_UTILS_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "ios/web/public/browser_state.h"

// Returns a basic FakeConsentAuditor.
// Allows to override factories in tests.
std::unique_ptr<KeyedService> BuildFakeConsentAuditor(
    web::BrowserState* context);

#endif  // IOS_CHROME_BROWSER_CONSENT_AUDITOR_MODEL_CONSENT_AUDITOR_TEST_UTILS_H_
