// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_NAVIGATOR_FONTS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_NAVIGATOR_FONTS_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ExceptionState;
class FontManager;
class Navigator;
class ScriptState;

class NavigatorFonts final {
  STATIC_ONLY(NavigatorFonts);

 public:
  static FontManager* fonts(ScriptState*, Navigator&, ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_NAVIGATOR_FONTS_H_
