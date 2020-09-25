// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_theme_default.h"

namespace blink {
namespace {

// There are no special themes on Fuchsia.
class LayoutThemeFuchsia : public LayoutThemeDefault {
 public:
  static scoped_refptr<LayoutTheme> Create() {
    return base::AdoptRef(new LayoutThemeFuchsia());
  }
};

}  // namespace

LayoutTheme& LayoutTheme::NativeTheme() {
  DEFINE_STATIC_REF(LayoutTheme, layout_theme, (LayoutThemeFuchsia::Create()));
  return *layout_theme;
}

}  // namespace blink
