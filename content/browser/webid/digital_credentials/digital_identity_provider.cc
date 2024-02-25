// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/digital_credentials/digital_identity_provider.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/webid/digital_credentials/digital_identity_provider_android.h"
#endif

namespace content {

DigitalIdentityProvider::DigitalIdentityProvider() = default;
DigitalIdentityProvider::~DigitalIdentityProvider() = default;

// static
std::unique_ptr<DigitalIdentityProvider> DigitalIdentityProvider::Create() {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<DigitalIdentityProviderAndroid>();
#else
  return nullptr;
#endif
}

}  // namespace content
