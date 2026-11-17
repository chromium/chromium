// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/test/stub_digital_identity_provider.h"

#include "content/public/browser/digital_identity_provider.h"

namespace content {

StubDigitalIdentityProvider::StubDigitalIdentityProvider() = default;
StubDigitalIdentityProvider::~StubDigitalIdentityProvider() = default;

bool StubDigitalIdentityProvider::IsLastCommittedOriginLowRisk(
    RenderFrameHost& render_frame_host) const {
  return false;
}

void StubDigitalIdentityProvider::Get(WebContents*,
                                      const url::Origin& origin,
                                      base::ValueView request,
                                      UiWarningLevel warning_level,
                                      DigitalIdentityCallback) {}

void StubDigitalIdentityProvider::Create(WebContents*,
                                         const url::Origin& origin,
                                         base::ValueView request,
                                         DigitalIdentityCallback) {}

}  // namespace content
