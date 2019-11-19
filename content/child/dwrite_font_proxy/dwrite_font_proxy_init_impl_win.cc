// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/dwrite_font_proxy/dwrite_font_proxy_init_impl_win.h"

#include <dwrite.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/alias.h"
#include "base/trace_event/trace_event.h"
#include "base/win/iat_patch_function.h"
#include "base/win/windows_version.h"
#include "content/child/dwrite_font_proxy/dwrite_font_proxy_win.h"
#include "content/child/dwrite_font_proxy/font_fallback_win.h"
#include "content/child/font_warmup_win.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/service_names.mojom.h"
#include "skia/ext/fontmgr_default.h"
#include "third_party/blink/public/web/win/web_font_rendering.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

namespace mswr = Microsoft::WRL;

namespace content {

namespace {

// Created on demand and then kept around until process exit.
DWriteFontCollectionProxy* g_font_collection = nullptr;
FontFallback* g_font_fallback = nullptr;

base::RepeatingCallback<mojo::PendingRemote<blink::mojom::DWriteFontProxy>(
    void)>* g_connection_callback_override = nullptr;

// Windows-only DirectWrite support. These warm up the DirectWrite paths
// before sandbox lock down to allow Skia access to the Font Manager service.
void CreateDirectWriteFactory(IDWriteFactory** factory) {
  // This shouldn't be necessary, but not having this causes breakage in
  // content_browsertests, and possibly other high-stress cases.
  PatchServiceManagerCalls();

  CHECK(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED,
                                      __uuidof(IDWriteFactory),
                                      reinterpret_cast<IUnknown**>(factory))));
}

}  // namespace

void InitializeDWriteFontProxy() {
  TRACE_EVENT0("dwrite,fonts", "InitializeDWriteFontProxy");
  mswr::ComPtr<IDWriteFactory> factory;

  CreateDirectWriteFactory(&factory);

  if (!g_font_collection) {
    mojo::PendingRemote<blink::mojom::DWriteFontProxy> dwrite_font_proxy;
    if (g_connection_callback_override) {
      dwrite_font_proxy = g_connection_callback_override->Run();
    } else if (auto* thread = ChildThread::Get()) {
      thread->BindHostReceiver(
          dwrite_font_proxy.InitWithNewPipeAndPassReceiver());
    }
    DWriteFontCollectionProxy::Create(&g_font_collection, factory.Get(),
                                      std::move(dwrite_font_proxy));
  }

  mswr::ComPtr<IDWriteFactory2> factory2;

  if (SUCCEEDED(factory.As(&factory2)) && factory2.Get()) {
    if (g_font_fallback)
      g_font_fallback->Release();
    FontFallback::Create(&g_font_fallback, g_font_collection);
  }

  sk_sp<SkFontMgr> skia_font_manager = SkFontMgr_New_DirectWrite(
      factory.Get(), g_font_collection, g_font_fallback);
  blink::WebFontRendering::SetSkiaFontManager(skia_font_manager);

  skia::OverrideDefaultSkFontMgr(std::move(skia_font_manager));

  // When IDWriteFontFallback is not available (prior to Win8.1) Skia will
  // still attempt to use DirectWrite to determine fallback fonts (in
  // SkFontMgr_DirectWrite::onMatchFamilyStyleCharacter), which will likely
  // result in trying to load the system font collection. To avoid that and
  // instead fall back on WebKit's fallback logic, we don't use Skia's font
  // fallback if IDWriteFontFallback is not available.
  // This flag can be removed when Win8.0 and earlier are no longer supported.
  bool fallback_available = g_font_fallback != nullptr;
  DCHECK_EQ(fallback_available,
            base::win::GetVersion() > base::win::Version::WIN8);
  blink::WebFontRendering::SetUseSkiaFontFallback(fallback_available);
}

void UninitializeDWriteFontProxy() {
  if (g_font_collection)
    g_font_collection->Unregister();
}

void SetDWriteFontProxySenderForTesting(
    base::RepeatingCallback<
        mojo::PendingRemote<blink::mojom::DWriteFontProxy>(void)> sender) {
  DCHECK(!g_connection_callback_override);
  g_connection_callback_override = new base::RepeatingCallback<
      mojo::PendingRemote<blink::mojom::DWriteFontProxy>(void)>(
      std::move(sender));
}

void ClearDWriteFontProxySenderForTesting() {
  delete g_connection_callback_override;
  g_connection_callback_override = nullptr;
}

}  // namespace content
