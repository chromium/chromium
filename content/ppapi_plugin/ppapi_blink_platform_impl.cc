// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/ppapi_plugin/ppapi_blink_platform_impl.h"

#include <stdint.h>

#include <map>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "third_party/blink/public/platform/web_storage_namespace.h"
#include "third_party/blink/public/platform/web_string.h"

#if defined(OS_MACOSX)
#include "third_party/blink/public/platform/mac/web_sandbox_support.h"
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
#include "content/child/child_process_sandbox_support_impl_linux.h"
#include "third_party/blink/public/platform/linux/out_of_process_font.h"
#include "third_party/blink/public/platform/linux/web_sandbox_support.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#endif

using blink::WebSandboxSupport;
using blink::WebString;
using blink::WebUChar;
using blink::WebUChar32;

typedef struct CGFont* CGFontRef;

namespace content {

#if !defined(OS_ANDROID) && !defined(OS_WIN)

class PpapiBlinkPlatformImpl::SandboxSupport : public WebSandboxSupport {
 public:
#if defined(OS_LINUX)
  explicit SandboxSupport(sk_sp<font_service::FontLoader> font_loader)
      : font_loader_(std::move(font_loader)) {}
#endif
  ~SandboxSupport() override {}

#if defined(OS_MACOSX)
  bool LoadFont(CTFontRef srcFont, CGFontRef* out, uint32_t* fontID) override;
#elif defined(OS_LINUX)
  SandboxSupport();
  void GetFallbackFontForCharacter(
      WebUChar32 character,
      const char* preferred_locale,
      blink::OutOfProcessFont* fallbackFont) override;
  void MatchFontByPostscriptNameOrFullFontName(
      const char* font_unique_name,
      blink::OutOfProcessFont* fallback_font) override;
  void GetWebFontRenderStyleForStrike(const char* family,
                                      int size,
                                      bool is_bold,
                                      bool is_italic,
                                      float device_scale_factor,
                                      blink::WebFontRenderStyle* out) override;

 private:
  // WebKit likes to ask us for the correct font family to use for a set of
  // unicode code points. It needs this information frequently so we cache it
  // here.
  std::map<int32_t, blink::OutOfProcessFont> unicode_font_families_;
  sk_sp<font_service::FontLoader> font_loader_;
  // For debugging https://crbug.com/312965
  base::SequenceCheckerImpl creation_thread_sequence_checker_;
#endif
};

#if defined(OS_MACOSX)

bool PpapiBlinkPlatformImpl::SandboxSupport::LoadFont(CTFontRef src_font,
                                                      CGFontRef* out,
                                                      uint32_t* font_id) {
  // TODO(brettw) this should do the something similar to what
  // RendererBlinkPlatformImpl does and request that the browser load the font.
  // Note: need to unlock the proxy lock like ensureFontLoaded does.
  NOTIMPLEMENTED();
  return false;
}

#elif defined(OS_POSIX)

PpapiBlinkPlatformImpl::SandboxSupport::SandboxSupport() {}

void PpapiBlinkPlatformImpl::SandboxSupport::GetFallbackFontForCharacter(
    WebUChar32 character,
    const char* preferred_locale,
    blink::OutOfProcessFont* fallbackFont) {
  ppapi::ProxyLock::AssertAcquired();
  // For debugging crbug.com/312965
  CHECK(creation_thread_sequence_checker_.CalledOnValidSequence());
  const std::map<int32_t, blink::OutOfProcessFont>::const_iterator iter =
      unicode_font_families_.find(character);
  if (iter != unicode_font_families_.end()) {
    fallbackFont->name = iter->second.name;
    fallbackFont->filename = iter->second.filename;
    fallbackFont->fontconfig_interface_id =
        iter->second.fontconfig_interface_id;
    fallbackFont->ttc_index = iter->second.ttc_index;
    fallbackFont->is_bold = iter->second.is_bold;
    fallbackFont->is_italic = iter->second.is_italic;
    return;
  }

  content::GetFallbackFontForCharacter(font_loader_, character,
                                       preferred_locale, fallbackFont);
  unicode_font_families_.insert(std::make_pair(character, *fallbackFont));
}

void PpapiBlinkPlatformImpl::SandboxSupport::GetWebFontRenderStyleForStrike(
    const char* family,
    int size,
    bool is_bold,
    bool is_italic,
    float device_scale_factor,
    blink::WebFontRenderStyle* out) {
  GetRenderStyleForStrike(font_loader_, family, size, is_bold, is_italic,
                          device_scale_factor, out);
}

void PpapiBlinkPlatformImpl::SandboxSupport::
    MatchFontByPostscriptNameOrFullFontName(
        const char* font_unique_name,
        blink::OutOfProcessFont* uniquely_matched_font) {
  content::MatchFontByPostscriptNameOrFullFontName(
      font_loader_, font_unique_name, uniquely_matched_font);
}

#endif

#endif  // !defined(OS_ANDROID) && !defined(OS_WIN)

PpapiBlinkPlatformImpl::PpapiBlinkPlatformImpl() {
#if defined(OS_LINUX)
  font_loader_ =
      sk_make_sp<font_service::FontLoader>(ChildThread::Get()->GetConnector());
  SkFontConfigInterface::SetGlobal(font_loader_);
  sandbox_support_.reset(
      new PpapiBlinkPlatformImpl::SandboxSupport(font_loader_));
#elif defined(OS_MACOSX)
  sandbox_support_.reset(new PpapiBlinkPlatformImpl::SandboxSupport());
#endif
}

PpapiBlinkPlatformImpl::~PpapiBlinkPlatformImpl() {
}

void PpapiBlinkPlatformImpl::Shutdown() {
#if !defined(OS_ANDROID) && !defined(OS_WIN)
  // SandboxSupport contains a map of OutOfProcessFont objects, which hold
  // WebStrings and WebVectors, which become invalidated when blink is shut
  // down. Hence, we need to clear that map now, just before blink::shutdown()
  // is called.
  sandbox_support_.reset();
#endif
}

blink::WebSandboxSupport* PpapiBlinkPlatformImpl::GetSandboxSupport() {
#if !defined(OS_ANDROID) && !defined(OS_WIN)
  return sandbox_support_.get();
#else
  return nullptr;
#endif
}

unsigned long long PpapiBlinkPlatformImpl::VisitedLinkHash(
    const char* canonical_url,
    size_t length) {
  NOTREACHED();
  return 0;
}

bool PpapiBlinkPlatformImpl::IsLinkVisited(unsigned long long link_hash) {
  NOTREACHED();
  return false;
}

blink::WebString PpapiBlinkPlatformImpl::DefaultLocale() {
  return blink::WebString::FromUTF8("en");
}

blink::WebThemeEngine* PpapiBlinkPlatformImpl::ThemeEngine() {
  NOTREACHED();
  return nullptr;
}

blink::WebData PpapiBlinkPlatformImpl::GetDataResource(const char* name) {
  NOTREACHED();
  return blink::WebData();
}

std::unique_ptr<blink::WebStorageNamespace>
PpapiBlinkPlatformImpl::CreateLocalStorageNamespace() {
  NOTREACHED();
  return nullptr;
}

int PpapiBlinkPlatformImpl::DatabaseDeleteFile(
    const blink::WebString& vfs_file_name,
    bool sync_dir) {
  NOTREACHED();
  return 0;
}

}  // namespace content
