// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_blink_platform_with_sandbox_support_impl.h"

#include "build/build_config.h"
#include "content/public/utility/utility_thread.h"

#if BUILDFLAG(IS_MAC)
#include "content/child/child_process_sandbox_support_impl_mac.h"
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "content/child/child_process_sandbox_support_impl_linux.h"
#endif

namespace content {

UtilityBlinkPlatformWithSandboxSupportImpl::
    UtilityBlinkPlatformWithSandboxSupportImpl() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  mojo::PendingRemote<font_service::mojom::FontService> font_service;
  UtilityThread::Get()->BindHostReceiver(
      font_service.InitWithNewPipeAndPassReceiver());
  sk_sp<font_service::FontLoader> font_loader =
      sk_make_sp<font_service::FontLoader>(std::move(font_service));
  SkFontConfigInterface::SetGlobal(font_loader);
  sandbox_support_ = std::make_unique<WebSandboxSupportLinux>(font_loader);
#elif BUILDFLAG(IS_MAC)
  sandbox_support_ = std::make_unique<WebSandboxSupportMac>();
#endif
}

UtilityBlinkPlatformWithSandboxSupportImpl::
    ~UtilityBlinkPlatformWithSandboxSupportImpl() {}

blink::WebSandboxSupport*
UtilityBlinkPlatformWithSandboxSupportImpl::GetSandboxSupport() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  return sandbox_support_.get();
#else
  return nullptr;
#endif
}

}  // namespace content
