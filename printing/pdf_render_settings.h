// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_RENDER_SETTINGS_H_
#define PRINTING_PDF_RENDER_SETTINGS_H_

#include "build/build_config.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

// Defining PDF rendering settings.
struct PdfRenderSettings {
  enum Mode {
    NORMAL = 0,
#if BUILDFLAG(IS_WIN)
    TEXTONLY,
    POSTSCRIPT_LEVEL2,
    POSTSCRIPT_LEVEL3,
    EMF_WITH_REDUCED_RASTERIZATION,
    POSTSCRIPT_LEVEL3_WITH_TYPE42_FONTS,
    LAST = POSTSCRIPT_LEVEL3_WITH_TYPE42_FONTS
#else
    LAST = NORMAL
#endif
  };

  PdfRenderSettings()
      : autorotate(false), use_color(true), mode(Mode::NORMAL) {}
  PdfRenderSettings(const gfx::Rect& area,
                    const gfx::Point& offsets,
                    const gfx::Size& dpi,
                    bool autorotate,
                    bool use_color,
                    Mode mode)
      : area(area),
        offsets(offsets),
        dpi(dpi),
        autorotate(autorotate),
        use_color(use_color),
        mode(mode) {}
  ~PdfRenderSettings() {}

  gfx::Rect area;
  gfx::Point offsets;
  gfx::Size dpi;
  bool autorotate;
  bool use_color;
  Mode mode;
};

}  // namespace printing

#endif  // PRINTING_PDF_RENDER_SETTINGS_H_
