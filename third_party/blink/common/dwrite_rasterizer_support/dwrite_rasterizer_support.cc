// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/dwrite_rasterizer_support/dwrite_rasterizer_support.h"
#include "base/logging.h"
#include "base/win/windows_version.h"
#include "ui/gfx/win/direct_write.h"

#include <dwrite.h>
#include <dwrite_2.h>
#include <wrl.h>

namespace blink {

bool DWriteRasterizerSupport::IsDWriteFactory2Available() {
  Microsoft::WRL::ComPtr<IDWriteFactory> factory;
  gfx::win::CreateDWriteFactory(&factory);
  Microsoft::WRL::ComPtr<IDWriteFactory2> factory2;
  factory.As<IDWriteFactory2>(&factory2);
  if (!factory2.Get()) {
    // If we were unable to get a IDWriteFactory2, check that we are actually on
    // a Windows version where we allow it. Windows 8.1 and up should have the
    // IDWritefactory2 available.
    CHECK_LT(base::win::GetVersion(), base::win::Version::WIN8_1);
  }
  return factory2.Get();
}

}  // namespace blink
