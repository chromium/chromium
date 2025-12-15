/*
 * Copyright (c) 2006,2007,2008, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// All of the functions in this file should move to new homes and this file
// should be deleted.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_SKIA_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_SKIA_UTILS_H_

#include "components/viz/common/resources/shared_image_format.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"

namespace blink {

bool PLATFORM_EXPORT
ApproximatelyEqualSkColorSpaces(sk_sp<SkColorSpace> src_color_space,
                                sk_sp<SkColorSpace> dst_color_space);

// Temporary utility while converting canvas code to use gfx::ColorSpace.
// TODO(crbug.com/371227617): Remove this once conversion is complete.
inline gfx::ColorSpace SkColorSpaceToGfxColorSpace(
    sk_sp<SkColorSpace> sk_color_space) {
  return sk_color_space ? gfx::ColorSpace(*sk_color_space)
                        : gfx::ColorSpace::CreateSRGB();
}

// Temporary utility while converting canvas code to use SharedImageFormat.
// TODO(crbug.com/391903236): Determine best long-term plan once canvas code is
// completely converted to SharedImageFormat (i.e., crbug.com/371227617 is
// resolved).
inline viz::SharedImageFormat GetN32FormatForCanvas() {
  return viz::SharedImageFormat::N32Format();
}

// Attempts to allocate an SkData on the PartitionAlloc buffer partition.
// If this fails (e.g. due to low memory), returns a null sk_sp<SkData> instead.
// Otherwise, the returned buffer is guaranteed to be zero-filled.
PLATFORM_EXPORT sk_sp<SkData> TryAllocateSkData(size_t size);

// Skia's smart pointer APIs are preferable over their legacy raw pointer
// counterparts.
//
// General guidelines
//
// When receiving ref counted objects from Skia:
//
//   1) Use sk_sp-based Skia factories if available (e.g. SkShader::MakeFoo()
//      instead of SkShader::CreateFoo()).
//   2) Use sk_sp<T> locals for all objects.
//
// When passing ref counted objects to Skia:
//
//   1) Use sk_sp-based Skia APIs when available (e.g.
//      SkPaint::setShader(sk_sp<SkShader>) instead of
//      SkPaint::setShader(SkShader*)).
//   2) If object ownership is being passed to Skia, use std::move(sk_sp<T>).
//
// Example (creating a SkShader and setting it on SkPaint):
//
// a) ownership transferred
//
//     // using Skia smart pointer locals
//     sk_sp<SkShader> shader = SkShader::MakeFoo(...);
//     paint.setShader(std::move(shader));
//
//     // using no locals
//     paint.setShader(SkShader::MakeFoo(...));
//
// b) shared ownership
//
//     sk_sp<SkShader> shader = SkShader::MakeFoo(...);
//     paint.setShader(shader);

// TODO(dcheng): This one might have some value, though it's not entirely clear
// if there is a specific issue with thread-hostility here. We transfer other
// mutable references across threads all the time, e.g. there is a broad
// allowance for scoped_refptr<T> as long as T is RefCountedThreadSafe.

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_SKIA_UTILS_H_
