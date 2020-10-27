// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/credential_provider/credential_provider_support.h"

#include "ios/chrome/browser/credential_provider/credential_provider_buildflags.h"

bool IsCredentialProviderExtensionSupported() {
  return BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED);
}
