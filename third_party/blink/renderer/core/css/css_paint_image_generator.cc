// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_paint_image_generator.h"

namespace blink {

namespace {

CSSPaintImageGenerator::CSSPaintImageGeneratorCreateFunction g_create_function =
    nullptr;

}  // namespace

// static
void CSSPaintImageGenerator::Init(
    CSSPaintImageGeneratorCreateFunction create_function) {
  DCHECK(!g_create_function);
  g_create_function = create_function;
}

// static
CSSPaintImageGenerator* CSSPaintImageGenerator::Create(const String& name,
                                                       const Document& document,
                                                       Observer* observer) {
  DCHECK(g_create_function);
  return g_create_function(name, document, observer);
}

// static
CSSPaintImageGenerator::CSSPaintImageGeneratorCreateFunction*
CSSPaintImageGenerator::GetCreateFunctionForTesting() {
  return &g_create_function;
}

CSSPaintImageGenerator::~CSSPaintImageGenerator() = default;

}  // namespace blink
