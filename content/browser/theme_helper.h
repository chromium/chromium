// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_THEME_HELPER_H_
#define CONTENT_BROWSER_THEME_HELPER_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/scoped_observer.h"
#include "content/common/renderer.mojom-forward.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

namespace content {

// This class is used to monitor system color info changes and to notify the
// renderer processes.
class ThemeHelper : public ui::NativeThemeObserver {
 public:
  static ThemeHelper* GetInstance();

  void SendSystemColorInfo(mojom::Renderer* renderer) const;

 private:
  friend class base::NoDestructor<ThemeHelper>;
  ThemeHelper();
  ~ThemeHelper() override;

  // Overridden from ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* updated_theme) override;

  ScopedObserver<ui::NativeTheme, ui::NativeThemeObserver> theme_observer_;

  DISALLOW_COPY_AND_ASSIGN(ThemeHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_THEME_HELPER_H_
