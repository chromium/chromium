// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_LINUX_H_
#define CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_LINUX_H_

#include <stdint.h>

#include <map>

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

  // |fallback_font| will be filled with a font family which provides glyphs for
  // the Unicode code point specified by |character|, a UTF-32 character.
  // |preferred_locale| contains the preferred locale identifier for
  // |character|. Returns false if the request could not be satisfied.
  bool GetFallbackFontForCharacter(
      blink::WebUChar32 character,
      const char* preferred_locale,
      gfx::FallbackFontData* fallback_font) override;

  // Matches a font uniquely by postscript name or full font name.  Used in
  // Blink for @font-face { src: local(arg) } matching.  Provide full font name
  // or postscript name as argument font_unique_name in UTF-8. |fallback_font|
  // contains a filename and fontconfig interface id if a match was found.
  // Returns false, otherwise.
  bool MatchFontByPostscriptNameOrFullFontName(
      const char* font_unique_name,
      gfx::FallbackFontData* fallback_font) override;

  // Returns rendering settings for a provided font family, size, and style.
  // |size_and_style| stores the bold setting in its least-significant bit, the
  // italic setting in its second-least-significant bit, and holds the requested
  // size in pixels into its remaining bits.
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

  sk_sp<font_service::FontLoader> font_loader_;
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_LINUX_H_
