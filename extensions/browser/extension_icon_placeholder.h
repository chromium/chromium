// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_ICON_PLACEHOLDER_H_
#define EXTENSIONS_BROWSER_EXTENSION_ICON_PLACEHOLDER_H_

#include <string>

#include "extensions/common/constants.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"

namespace gfx {
class Canvas;
}

namespace extensions {

// An extension icon image with a gray background and the first letter of the
// extension name, so that not all extensions without an icon look the same.
class ExtensionIconPlaceholder : public gfx::CanvasImageSource {
 public:
  ExtensionIconPlaceholder(extension_misc::ExtensionIcons size,
                           const std::string& name);

  ExtensionIconPlaceholder(const ExtensionIconPlaceholder&) = delete;
  ExtensionIconPlaceholder& operator=(const ExtensionIconPlaceholder&) = delete;

  ~ExtensionIconPlaceholder() override;

  // Creates an image backed by an ImageSkia with the ExtensionIconPlaceholder
  // as its image source.
  static gfx::Image CreateImage(extension_misc::ExtensionIcons size,
                                const std::string& name);

 private:
  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;

  // The size this placeholder is filling.
  extension_misc::ExtensionIcons icon_size_;

  // The first letter of the extension's name.
  std::u16string letter_;

  // The gray background image, on top of which the letter is drawn.
  gfx::Image base_image_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_ICON_PLACEHOLDER_H_
