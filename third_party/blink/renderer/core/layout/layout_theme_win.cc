// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/no_destructor.h"
#include "third_party/blink/renderer/core/layout/layout_theme_default.h"

namespace blink {
namespace {

class LayoutThemeWin final : public LayoutThemeDefault {
 public:
  static scoped_refptr<LayoutTheme> Create() {
    return base::AdoptRef(new LayoutThemeWin());
  }
};

}  // namespace

LayoutTheme& LayoutTheme::NativeTheme() {
  static const base::NoDestructor<scoped_refptr<LayoutTheme>> layout_theme(
      LayoutThemeWin::Create());
  return **layout_theme;
}

}  // namespace blink
