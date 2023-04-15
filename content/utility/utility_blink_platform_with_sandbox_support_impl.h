// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_UTILITY_BLINK_PLATFORM_WITH_SANDBOX_SUPPORT_IMPL_H_
#define CONTENT_UTILITY_UTILITY_BLINK_PLATFORM_WITH_SANDBOX_SUPPORT_IMPL_H_

#include <memory>

#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"

namespace blink {
class WebSandboxSupport;
}

namespace content {

// This class extends from UtilityBlinkPlatformImpl with added blink web
// sandbox support.
class UtilityBlinkPlatformWithSandboxSupportImpl : public blink::Platform {
 public:
  UtilityBlinkPlatformWithSandboxSupportImpl();

  UtilityBlinkPlatformWithSandboxSupportImpl(
      const UtilityBlinkPlatformWithSandboxSupportImpl&) = delete;
  UtilityBlinkPlatformWithSandboxSupportImpl& operator=(
      const UtilityBlinkPlatformWithSandboxSupportImpl&) = delete;

  ~UtilityBlinkPlatformWithSandboxSupportImpl() override;

  // BlinkPlatformImpl
  blink::WebSandboxSupport* GetSandboxSupport() override;

 private:
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  std::unique_ptr<blink::WebSandboxSupport> sandbox_support_;
#endif
};

}  // namespace content

#endif  // CONTENT_UTILITY_UTILITY_BLINK_PLATFORM_WITH_SANDBOX_SUPPORT_IMPL_H_
