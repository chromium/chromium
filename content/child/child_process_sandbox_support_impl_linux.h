// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_LINUX_H_
#define CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_LINUX_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/services/font/public/cpp/font_loader.h"
#include "third_party/blink/public/platform/linux/web_sandbox_support.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/font_fallback_linux.h"

namespace blink {
struct WebFontRenderStyle;
}

namespace content {

// Child-process implementation of the Blink interface that sandboxed processes
// use to obtain data from the privileged process (browser), which would
// otherwise be blocked by the sandbox.
class WebSandboxSupportLinux : public blink::WebSandboxSupport {
 public:
  explicit WebSandboxSupportLinux(sk_sp<font_service::FontLoader> font_loader);

  WebSandboxSupportLinux(const WebSandboxSupportLinux&) = delete;
  WebSandboxSupportLinux& operator=(const WebSandboxSupportLinux&) = delete;

  ~WebSandboxSupportLinux() override;

  // blink::WebSandboxSupport:
  bool GetFallbackFontForCharacter(
      blink::WebUChar32 character,
      const char* preferred_locale,
      gfx::FallbackFontData* fallback_font) override;
  bool MatchFontByPostscriptNameOrFullFontName(
      const char* font_unique_name,
      gfx::FallbackFontData* fallback_font) override;
  void GetWebFontRenderStyleForStrike(const char* family,
                                      int size,
                                      bool is_bold,
                                      bool is_italic,
                                      float device_scale_factor,
                                      blink::WebFontRenderStyle* out) override;

 private:
  // Blink calls GetFallbackFontForCharacter frequently, so the results are
  // cached. The cache is protected by this lock.
  base::Lock lock_;
  // Maps unicode chars to their fallback fonts.
  std::map<int32_t, gfx::FallbackFontData> unicode_font_families_
      GUARDED_BY(lock_);
  // Characters (with the locale they were requested for) for which the browser
  // found no fallback font. Without this every occurrence of such a character
  // costs a round trip to the browser each time the text containing it is
  // shaped; with no color emoji font installed that is every emoji, since
  // Blink first asks for an emoji presentation font.
  std::set<std::pair<int32_t, std::string>> no_fallback_font_ GUARDED_BY(lock_);

  const sk_sp<font_service::FontLoader> font_loader_;
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_LINUX_H_
