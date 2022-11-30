// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/background_color_paint_image_generator.h"

namespace blink {

namespace {

BackgroundColorPaintImageGenerator::
    BackgroundColorPaintImageGeneratorCreateFunction g_create_function =
        nullptr;

}  // namespace

// static
void BackgroundColorPaintImageGenerator::Init(
    BackgroundColorPaintImageGeneratorCreateFunction create_function) {
  DCHECK(!g_create_function);
  g_create_function = create_function;
}

BackgroundColorPaintImageGenerator* BackgroundColorPaintImageGenerator::Create(
    LocalFrame& local_root) {
  DCHECK(g_create_function);
  return g_create_function(local_root);
}

}  // namespace blink
