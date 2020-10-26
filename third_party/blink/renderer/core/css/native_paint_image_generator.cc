// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/native_paint_image_generator.h"

namespace blink {

namespace {

NativePaintImageGenerator::NativePaintImageGeneratorCreateFunction
    g_create_function = nullptr;

}  // namespace

// static
void NativePaintImageGenerator::Init(
    NativePaintImageGeneratorCreateFunction create_function) {
  DCHECK(!g_create_function);
  g_create_function = create_function;
}

// static
std::unique_ptr<NativePaintImageGenerator> NativePaintImageGenerator::Create() {
  DCHECK(g_create_function);
  return g_create_function();
}

NativePaintImageGenerator::~NativePaintImageGenerator() = default;

}  // namespace blink
