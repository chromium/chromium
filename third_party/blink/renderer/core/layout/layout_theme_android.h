// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_ANDROID_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_ANDROID_H_

#include "third_party/blink/renderer/core/layout/layout_theme_mobile.h"

namespace blink {

class LayoutThemeAndroid final : public LayoutThemeMobile {
 public:
  static scoped_refptr<LayoutTheme> Create();
  bool DelegatesMenuListRendering() const override { return true; }
  String ExtraDefaultStyleSheet() override;

 private:
  ~LayoutThemeAndroid() override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_ANDROID_H_
