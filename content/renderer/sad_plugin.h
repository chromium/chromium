// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SAD_PLUGIN_H_
#define CONTENT_RENDERER_SAD_PLUGIN_H_

#include "cc/paint/paint_canvas.h"

namespace cc {
class PaintImage;
}

namespace gfx {
class Rect;
}

namespace content {

// Paints the sad plugin to the given canvas for the given plugin bounds. This
// is used by PPAPI out-of-process plugin impls.
void PaintSadPlugin(cc::PaintCanvas* canvas,
                    const gfx::Rect& plugin_rect,
                    const cc::PaintImage& sad_plugin_image);

}  // namespace content

#endif  // CONTENT_RENDERER_SAD_PLUGIN_H_
