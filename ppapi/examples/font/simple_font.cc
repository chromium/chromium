// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdio.h>

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/trusted/browser_font_trusted.h"

static void DummyCompletionCallback(void* /*user_data*/, int32_t /*result*/) {
}

class MyInstance : public pp::Instance {
 public:
  MyInstance(PP_Instance instance)
      : pp::Instance(instance) {
  }

  virtual void DidChangeView(const pp::Rect& position, const pp::Rect& clip) {
    if (position.size() == last_size_)
      return;
    last_size_ = position.size();

    pp::ImageData image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL, last_size_, true);
    pp::Graphics2D graphics(this, last_size_, false);
    BindGraphics(graphics);

    {
      pp::BrowserFontDescription desc;
      desc.set_family(PP_BROWSERFONT_TRUSTED_FAMILY_SANSSERIF);
      desc.set_size(100);
      pp::BrowserFont_Trusted font(this, desc);

      // Draw some large, alpha blended text, including Arabic shaping.
      pp::Rect text_clip(position.size());  // Use entire bounds for clip.
      font.DrawTextAt(
          &image,
          pp::BrowserFontTextRun(
              "\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7\xE2\x80\x8E", true,
              true),
          pp::Point(20, 100), 0x80008000, clip, false);
    }

    // Draw the default font names and sizes.
    int y = 160;
    {
      pp::BrowserFontDescription desc;
      pp::BrowserFont_Trusted default_font(this, desc);
      default_font.DrawSimpleText(
          &image, DescribeFont(default_font, "Default font"),
          pp::Point(10, y), 0xFF000000);
      y += 20;
    }
    {
      pp::BrowserFontDescription desc;
      desc.set_family(PP_BROWSERFONT_TRUSTED_FAMILY_SERIF);
      pp::BrowserFont_Trusted serif_font(this, desc);
      serif_font.DrawSimpleText(
          &image, DescribeFont(serif_font, "Serif font"),
          pp::Point(10, y), 0xFF000000);
      y += 20;
    }
    {
      pp::BrowserFontDescription desc;
      desc.set_family(PP_BROWSERFONT_TRUSTED_FAMILY_SANSSERIF);
      pp::BrowserFont_Trusted sans_serif_font(this, desc);
      sans_serif_font.DrawSimpleText(
          &image, DescribeFont(sans_serif_font, "Sans serif font"),
          pp::Point(10, y), 0xFF000000);
      y += 20;
    }
    {
      pp::BrowserFontDescription desc;
      desc.set_family(PP_BROWSERFONT_TRUSTED_FAMILY_MONOSPACE);
      pp::BrowserFont_Trusted monospace_font(this, desc);
      monospace_font.DrawSimpleText(
          &image, DescribeFont(monospace_font, "Monospace font"),
          pp::Point(10, y), 0xFF000000);
      y += 20;
    }

    graphics.PaintImageData(image, pp::Point(0, 0));
    graphics.Flush(pp::CompletionCallback(&DummyCompletionCallback, NULL));
  }

 private:
  // Returns a string describing the given font, using the given title.
  std::string DescribeFont(const pp::BrowserFont_Trusted& font,
                           const char* title) {
    pp::BrowserFontDescription desc;
    PP_BrowserFont_Trusted_Metrics metrics;
    font.Describe(&desc, &metrics);

    char buf[256];
    snprintf(buf, sizeof(buf), "%s = %s %dpt",
            title, desc.face().AsString().c_str(), desc.size());
    return std::string(buf);
  }

  pp::Size last_size_;
};

class MyModule : public pp::Module {
 public:
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
