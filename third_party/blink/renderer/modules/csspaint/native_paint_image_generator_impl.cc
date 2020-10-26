// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/native_paint_image_generator_impl.h"

#include "third_party/blink/renderer/modules/csspaint/native_paint_worklet.h"
#include "third_party/blink/renderer/platform/graphics/image.h"

namespace blink {

std::unique_ptr<NativePaintImageGenerator>
NativePaintImageGeneratorImpl::Create() {
  std::unique_ptr<NativePaintWorklet> native_paint_worklet =
      std::make_unique<NativePaintWorklet>();

  DCHECK(native_paint_worklet);
  std::unique_ptr<NativePaintImageGeneratorImpl> generator =
      std::make_unique<NativePaintImageGeneratorImpl>(
          std::move(native_paint_worklet));

  return generator;
}

NativePaintImageGeneratorImpl::NativePaintImageGeneratorImpl(
    std::unique_ptr<NativePaintWorklet> native_paint_worklet)
    : native_paint_worklet_(std::move(native_paint_worklet)) {}

NativePaintImageGeneratorImpl::~NativePaintImageGeneratorImpl() = default;

scoped_refptr<Image> NativePaintImageGeneratorImpl::Paint(
    const FloatSize& container_size,
    SkColor color) {
  return native_paint_worklet_->Paint(container_size, color);
}

}  // namespace blink
