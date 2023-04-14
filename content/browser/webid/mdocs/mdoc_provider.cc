// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/mdocs/mdoc_provider.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/webid/mdocs/mdoc_provider_android.h"
#endif

namespace content {

MDocProvider::MDocProvider() = default;
MDocProvider::~MDocProvider() = default;

// static
std::unique_ptr<MDocProvider> MDocProvider::Create() {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<MDocProviderAndroid>();
#else
  return nullptr;
#endif
}

}  // namespace content
