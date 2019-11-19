// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_UTILITY_BLINK_PLATFORM_WITH_SANDBOX_SUPPORT_IMPL_H_
#define CONTENT_UTILITY_UTILITY_BLINK_PLATFORM_WITH_SANDBOX_SUPPORT_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"

#if defined(OS_LINUX)
#include "components/services/font/public/cpp/font_loader.h"  // nogncheck
#include "third_party/skia/include/core/SkRefCnt.h"           // nogncheck
#endif

namespace blink {
class WebSandboxSupport;
}

namespace content {

// This class extends from UtilityBlinkPlatformImpl with added blink web
// sandbox support.
class UtilityBlinkPlatformWithSandboxSupportImpl : public blink::Platform {
 public:
  UtilityBlinkPlatformWithSandboxSupportImpl();
  ~UtilityBlinkPlatformWithSandboxSupportImpl() override;

  // BlinkPlatformImpl
  blink::WebSandboxSupport* GetSandboxSupport() override;

 private:
#if defined(OS_LINUX) || defined(OS_MACOSX)
  std::unique_ptr<blink::WebSandboxSupport> sandbox_support_;
#endif
#if defined(OS_LINUX)
  sk_sp<font_service::FontLoader> font_loader_;
#endif

  DISALLOW_COPY_AND_ASSIGN(UtilityBlinkPlatformWithSandboxSupportImpl);
};

}  // namespace content

#endif  // CONTENT_UTILITY_UTILITY_BLINK_PLATFORM_WITH_SANDBOX_SUPPORT_IMPL_H_
