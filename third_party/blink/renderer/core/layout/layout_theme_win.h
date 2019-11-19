// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_WIN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_WIN_H_

#include "third_party/blink/renderer/core/layout/layout_theme_default.h"

namespace blink {

class LayoutThemeWin final : public LayoutThemeDefault {
 public:
  static scoped_refptr<LayoutTheme> Create();

  Color SystemColor(CSSValueID css_value_id,
                    WebColorScheme color_scheme) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_WIN_H_
