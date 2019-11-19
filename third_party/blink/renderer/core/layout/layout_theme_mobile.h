/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_MOBILE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_MOBILE_H_

#include "third_party/blink/renderer/core/layout/layout_theme_default.h"

namespace blink {

class LayoutThemeMobile : public LayoutThemeDefault {
 public:
  static scoped_refptr<LayoutTheme> Create();
  String ExtraDefaultStyleSheet() override;

  void AdjustInnerSpinButtonStyle(ComputedStyle&) const override;

  String ExtraFullscreenStyleSheet() override;

  Color PlatformTapHighlightColor() const override {
    return LayoutThemeMobile::kDefaultTapHighlightColor;
  }

  Color PlatformActiveSelectionBackgroundColor(
      WebColorScheme color_scheme) const override {
    return LayoutThemeMobile::kDefaultActiveSelectionBackgroundColor;
  }

 protected:
  ~LayoutThemeMobile() override;
  bool ShouldUseFallbackTheme(const ComputedStyle&) const override;

 private:
  static const RGBA32 kDefaultTapHighlightColor = 0x6633b5e5;
  static const RGBA32 kDefaultActiveSelectionBackgroundColor = 0x6633b5e5;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_MOBILE_H_
