// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/dwrite_font_proxy/dwrite_font_proxy_init_impl_win.h"

#include <dwrite.h>

#include <utility>

#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/trace_event/trace_event.h"
#include "base/win/iat_patch_function.h"
#include "base/win/windows_version.h"
#include "content/child/dwrite_font_proxy/dwrite_font_proxy_win.h"
#include "content/child/dwrite_font_proxy/font_fallback_win.h"
#include "content/child/font_warmup_win.h"
#include "content/public/child/child_thread.h"
#include "skia/ext/font_utils.h"
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
    if (g_connection_callback_override)
      dwrite_font_proxy = g_connection_callback_override->Run();
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
  blink::WebFontRendering::SetFontRenderingClient(g_font_collection);

  skia::OverrideDefaultSkFontMgr(std::move(skia_font_manager));

  DCHECK(g_font_fallback);
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
