// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/digital_credentials/digital_credential_provider.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/webid/digital_credentials/digital_credential_provider_android.h"
#endif

namespace content {

DigitalCredentialProvider::DigitalCredentialProvider() = default;
DigitalCredentialProvider::~DigitalCredentialProvider() = default;

// static
std::unique_ptr<DigitalCredentialProvider> DigitalCredentialProvider::Create() {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<DigitalCredentialProviderAndroid>();
#else
  return nullptr;
#endif
}

}  // namespace content
