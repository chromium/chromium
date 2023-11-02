// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_DWRITE_FONT_PROXY_FONT_FALLBACK_WIN_H_
#define CONTENT_CHILD_DWRITE_FONT_PROXY_FONT_FALLBACK_WIN_H_

#include <dwrite.h>
#include <dwrite_2.h>
#include <wrl.h>

#include <list>
#include <map>

#include "content/child/dwrite_font_proxy/dwrite_font_proxy_win.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/dwrite_font_proxy/dwrite_font_proxy.mojom.h"

namespace content {

// Implements an  IDWriteFontFallback that uses IPC to proxy the fallback calls
// to the system fallback in the browser process.
class FontFallback
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDWriteFontFallback> {
 public:
  // Factory method to avoid exporting the class and all it derives from.
  static CONTENT_EXPORT HRESULT Create(FontFallback** font_fallback_out,
                                       DWriteFontCollectionProxy* collection);

  // Use Create() to construct these objects. Direct calls to the constructor
  // are an error - it is only public because a WRL helper function creates the
  // objects.
  FontFallback();

  FontFallback& operator=(const FontFallback&) = delete;

  HRESULT STDMETHODCALLTYPE
  MapCharacters(IDWriteTextAnalysisSource* source,
                UINT32 text_position,
                UINT32 text_length,
                IDWriteFontCollection* base_font_collection,
                const wchar_t* base_family_name,
                DWRITE_FONT_WEIGHT base_weight,
                DWRITE_FONT_STYLE base_style,
                DWRITE_FONT_STRETCH base_stretch,
                UINT32* mapped_length,
                IDWriteFont** mapped_font,
                FLOAT* scale) override;

  HRESULT STDMETHODCALLTYPE
  RuntimeClassInitialize(DWriteFontCollectionProxy* collection);

 protected:
  ~FontFallback() override;

  // Note: `mapped_font` is used in `SkFontMgr_DirectWrite::fallback()` to
  // construct `SkTypeface` which can be shared between threads[1].
  // [1] https://api.skia.org/classSkTypeface.html#details
  bool GetCachedFont(const std::u16string& text,
                     const wchar_t* base_family_name,
                     const wchar_t* locale,
                     DWRITE_FONT_WEIGHT base_weight,
                     DWRITE_FONT_STYLE base_style,
                     DWRITE_FONT_STRETCH base_stretch,
                     IDWriteFont** mapped_font,
                     size_t* mapped_length) LOCKS_EXCLUDED(lock_);

  void AddCachedFamily(Microsoft::WRL::ComPtr<IDWriteFontFamily> family,
                       const wchar_t* base_family_name,
                       const wchar_t* locale) LOCKS_EXCLUDED(lock_);

 private:
  blink::mojom::DWriteFontProxy& GetFontProxy();

  Microsoft::WRL::ComPtr<DWriteFontCollectionProxy> collection_;

  base::Lock lock_;

  // |fallback_family_cache_| keeps a mapping from base family name to a list
  // of font families that matched a character on a previous call. The list is
  // capped in size and maintained in MRU order. This gives us a good chance of
  // returning a suitable fallback font without having to do an IPC.
  std::map<std::wstring, std::list<Microsoft::WRL::ComPtr<IDWriteFontFamily>>>
      fallback_family_cache_ GUARDED_BY(lock_);
};

}  // namespace content

#endif  // CONTENT_CHILD_DWRITE_FONT_PROXY_FONT_FALLBACK_WIN_H_
