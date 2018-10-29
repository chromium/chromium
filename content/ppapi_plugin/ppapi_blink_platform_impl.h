// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PPAPI_PLUGIN_PPAPI_BLINK_PLATFORM_IMPL_H_
#define CONTENT_PPAPI_PLUGIN_PPAPI_BLINK_PLATFORM_IMPL_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/child/blink_platform_impl.h"

#if defined(OS_LINUX)
#include "components/services/font/public/cpp/font_loader.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#endif

namespace content {

class PpapiBlinkPlatformImpl : public BlinkPlatformImpl {
 public:
  PpapiBlinkPlatformImpl();
  ~PpapiBlinkPlatformImpl() override;

  // Shutdown must be called just prior to shutting down blink.
  void Shutdown();

  // BlinkPlatformImpl methods:
  blink::WebSandboxSupport* GetSandboxSupport() override;
  unsigned long long VisitedLinkHash(const char* canonical_url,
                                     size_t length) override;
  bool IsLinkVisited(unsigned long long link_hash) override;
  blink::WebString DefaultLocale() override;
  blink::WebThemeEngine* ThemeEngine() override;
  blink::WebData GetDataResource(const char* name) override;
  std::unique_ptr<blink::WebStorageNamespace> CreateLocalStorageNamespace()
      override;
  int DatabaseDeleteFile(const blink::WebString& vfs_file_name,
                         bool sync_dir) override;

 private:
#if !defined(OS_ANDROID) && !defined(OS_WIN)
  class SandboxSupport;
  std::unique_ptr<SandboxSupport> sandbox_support_;
#endif

#if defined(OS_LINUX)
  sk_sp<font_service::FontLoader> font_loader_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PpapiBlinkPlatformImpl);
};

}  // namespace content

#endif  // CONTENT_PPAPI_PLUGIN_PPAPI_BLINK_PLATFORM_IMPL_H_
