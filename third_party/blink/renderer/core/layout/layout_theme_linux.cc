// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_theme_linux.h"

#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"

namespace blink {

scoped_refptr<LayoutTheme> LayoutThemeLinux::Create() {
  return base::AdoptRef(new LayoutThemeLinux());
}

LayoutTheme& LayoutTheme::NativeTheme() {
  DEFINE_STATIC_REF(LayoutTheme, layout_theme, (LayoutThemeLinux::Create()));
  return *layout_theme;
}

String LayoutThemeLinux::ExtraDefaultStyleSheet() {
  return LayoutThemeDefault::ExtraDefaultStyleSheet() +
         UncompressResourceAsASCIIString(IDR_UASTYLE_THEME_CHROMIUM_LINUX_CSS);
}

}  // namespace blink
