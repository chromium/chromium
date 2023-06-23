// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/mock_internal_authenticator.h"

namespace webauthn {

MockInternalAuthenticator::MockInternalAuthenticator(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}
MockInternalAuthenticator::~MockInternalAuthenticator() = default;

}  // namespace webauthn
