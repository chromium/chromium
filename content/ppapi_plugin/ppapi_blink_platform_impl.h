// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PPAPI_PLUGIN_PPAPI_BLINK_PLATFORM_IMPL_H_
#define CONTENT_PPAPI_PLUGIN_PPAPI_BLINK_PLATFORM_IMPL_H_

#include <stddef.h>

#include <memory>
#include <string_view>

#include "build/build_config.h"
#include "content/child/blink_platform_impl.h"

namespace content {

class PpapiBlinkPlatformImpl : public BlinkPlatformImpl {
 public:
  PpapiBlinkPlatformImpl();

  PpapiBlinkPlatformImpl(const PpapiBlinkPlatformImpl&) = delete;
  PpapiBlinkPlatformImpl& operator=(const PpapiBlinkPlatformImpl&) = delete;

  ~PpapiBlinkPlatformImpl() override;

  // Shutdown must be called just prior to shutting down blink.
  void Shutdown();

  // BlinkPlatformImpl methods:
  blink::WebSandboxSupport* GetSandboxSupport() override;
  uint64_t VisitedLinkHash(std::string_view canonical_url) override;
  bool IsLinkVisited(uint64_t link_hash) override;
  void AddOrUpdateVisitedLinkSalt(const url::Origin& origin,
                                  uint64_t salt) override;
  blink::WebString DefaultLocale() override;

 private:
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  std::unique_ptr<blink::WebSandboxSupport> sandbox_support_;
#endif
};

}  // namespace content

#endif  // CONTENT_PPAPI_PLUGIN_PPAPI_BLINK_PLATFORM_IMPL_H_
