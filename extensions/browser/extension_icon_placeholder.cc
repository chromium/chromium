// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_icon_placeholder.h"

#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

namespace {

// Returns the FontStyle to use for the given icon |size|.
ui::ResourceBundle::FontStyle GetFontStyleForIconSize(
    extension_misc::ExtensionIcons size) {
  switch (size) {
    case extension_misc::EXTENSION_ICON_INVALID:
    case extension_misc::EXTENSION_ICON_BITTY:
      return ui::ResourceBundle::SmallFont;
    case extension_misc::EXTENSION_ICON_SMALLISH:
    case extension_misc::EXTENSION_ICON_SMALL:
      return ui::ResourceBundle::MediumFont;
    case extension_misc::EXTENSION_ICON_MEDIUM:
    case extension_misc::EXTENSION_ICON_LARGE:
    case extension_misc::EXTENSION_ICON_EXTRA_LARGE:
    case extension_misc::EXTENSION_ICON_GIGANTOR:
      return ui::ResourceBundle::LargeFont;
  }
  NOTREACHED();
  return ui::ResourceBundle::MediumFont;
}

// Returns the background image to use for the given icon |size|.
gfx::Image GetBackgroundImageForIconSize(extension_misc::ExtensionIcons size) {
  int resource_id = 0;
  // Right now, we have resources for a 19x19 (action) and a 48x48 (extensions
  // page icon). The implementation of the placeholder scales these correctly,
  // so it's not a big deal to use these for other sizes, but if it's something
  // that will be done frequently, we should probably make a devoted asset for
  // that size.
  switch (size) {
    case extension_misc::EXTENSION_ICON_INVALID:
    case extension_misc::EXTENSION_ICON_BITTY:
    case extension_misc::EXTENSION_ICON_SMALLISH:
    case extension_misc::EXTENSION_ICON_SMALL:
      resource_id = IDR_EXTENSION_ACTION_PLAIN_BACKGROUND;
      break;
    case extension_misc::EXTENSION_ICON_MEDIUM:
    case extension_misc::EXTENSION_ICON_LARGE:
    case extension_misc::EXTENSION_ICON_EXTRA_LARGE:
    case extension_misc::EXTENSION_ICON_GIGANTOR:
      resource_id = IDR_EXTENSION_ICON_PLAIN_BACKGROUND;
      break;
  }
  return ui::ResourceBundle::GetSharedInstance().GetImageNamed(resource_id);
}

}  // namespace

ExtensionIconPlaceholder::ExtensionIconPlaceholder(
    extension_misc::ExtensionIcons size,
    const std::string& name)
    : gfx::CanvasImageSource(gfx::Size(size, size)),
      icon_size_(size),
      base_image_(GetBackgroundImageForIconSize(size)) {
  // Remove RTL formatting characters, if any, that may pad the extension name.
  // See https://crbug.com/869358
  base::string16 sanitized_name = base::UTF8ToUTF16(std::string(name));
  base::i18n::UnadjustStringForLocaleDirection(&sanitized_name);

  letter_ = sanitized_name.substr(0, 1);
}

ExtensionIconPlaceholder::~ExtensionIconPlaceholder() {
}

gfx::Image ExtensionIconPlaceholder::CreateImage(
    extension_misc::ExtensionIcons size,
    const std::string& name) {
  return gfx::Image(
      gfx::ImageSkia(std::make_unique<ExtensionIconPlaceholder>(size, name),
                     gfx::Size(size, size)));
}

void ExtensionIconPlaceholder::Draw(gfx::Canvas* canvas) {
  // Draw the background image, correctly scaled.
  canvas->DrawImageInt(*base_image_.ToImageSkia(), 0, 0,
                       base_image_.Size().width(), base_image_.Size().height(),
                       0, 0, size().width(), size().height(), true);
  gfx::Rect bounds(size().width(), size().height());
  // Draw the letter on top.
  canvas->DrawStringRectWithFlags(
      letter_, ui::ResourceBundle::GetSharedInstance().GetFontList(
                   GetFontStyleForIconSize(icon_size_)),
      SK_ColorWHITE, bounds, gfx::Canvas::TEXT_ALIGN_CENTER);
}

}  // namespace extensions
