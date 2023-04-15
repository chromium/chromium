// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/ppapi_plugin/ppapi_blink_platform_impl.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/notreached.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "third_party/blink/public/platform/web_string.h"

#if BUILDFLAG(IS_MAC)
#include "content/child/child_process_sandbox_support_impl_mac.h"
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "content/child/child_process_sandbox_support_impl_linux.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#endif

using blink::WebSandboxSupport;
using blink::WebString;
using blink::WebUChar;
using blink::WebUChar32;

typedef struct CGFont* CGFontRef;

namespace content {

PpapiBlinkPlatformImpl::PpapiBlinkPlatformImpl() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  mojo::PendingRemote<font_service::mojom::FontService> font_service;
  ChildThread::Get()->BindHostReceiver(
      font_service.InitWithNewPipeAndPassReceiver());
  sk_sp<font_service::FontLoader> font_loader =
      sk_make_sp<font_service::FontLoader>(std::move(font_service));
  SkFontConfigInterface::SetGlobal(font_loader);
  sandbox_support_ = std::make_unique<WebSandboxSupportLinux>(font_loader);
#elif BUILDFLAG(IS_MAC)
  sandbox_support_ = std::make_unique<WebSandboxSupportMac>();
#endif
}

PpapiBlinkPlatformImpl::~PpapiBlinkPlatformImpl() {
}

void PpapiBlinkPlatformImpl::Shutdown() {}

blink::WebSandboxSupport* PpapiBlinkPlatformImpl::GetSandboxSupport() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  return sandbox_support_.get();
#else
  return nullptr;
#endif
}

uint64_t PpapiBlinkPlatformImpl::VisitedLinkHash(const char* canonical_url,
                                                 size_t length) {
  NOTREACHED();
  return 0;
}

bool PpapiBlinkPlatformImpl::IsLinkVisited(uint64_t link_hash) {
  NOTREACHED();
  return false;
}

blink::WebString PpapiBlinkPlatformImpl::DefaultLocale() {
  return blink::WebString::FromUTF8("en");
}

}  // namespace content
