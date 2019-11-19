/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_blend.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_gaussian_blur.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_merge.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/filters/source_graphic.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

using testing::Test;

namespace blink {

class ImageFilterBuilderTest : public Test {
 protected:
  void InterpolationSpaceTest() {
    // Build filter tree
    auto* reference_filter = MakeGarbageCollected<Filter>(1.0f);

    // Add a dummy source graphic input
    FilterEffect* source_effect = reference_filter->GetSourceGraphic();
    source_effect->SetOperatingInterpolationSpace(kInterpolationSpaceSRGB);

    // Add a blur effect (with input : source)
    auto* blur_effect =
        MakeGarbageCollected<FEGaussianBlur>(reference_filter, 3.0f, 3.0f);
    blur_effect->SetOperatingInterpolationSpace(kInterpolationSpaceLinear);
    blur_effect->InputEffects().push_back(source_effect);

    // Add a blend effect (with inputs : blur, source)
    auto* blend_effect =
        MakeGarbageCollected<FEBlend>(reference_filter, BlendMode::kNormal);
    blend_effect->SetOperatingInterpolationSpace(kInterpolationSpaceSRGB);
    FilterEffectVector& blend_inputs = blend_effect->InputEffects();
    blend_inputs.ReserveCapacity(2);
    blend_inputs.push_back(source_effect);
    blend_inputs.push_back(blur_effect);

    // Add a merge effect (with inputs : blur, blend)
    auto* merge_effect = MakeGarbageCollected<FEMerge>(reference_filter);
    merge_effect->SetOperatingInterpolationSpace(kInterpolationSpaceLinear);
    FilterEffectVector& merge_inputs = merge_effect->InputEffects();
    merge_inputs.ReserveCapacity(2);
    merge_inputs.push_back(blur_effect);
    merge_inputs.push_back(blend_effect);
    reference_filter->SetLastEffect(merge_effect);

    // Get PaintFilter resulting tree
    sk_sp<PaintFilter> filter = paint_filter_builder::Build(
        reference_filter->LastEffect(), kInterpolationSpaceSRGB);

    // Let's check that the resulting tree looks like this :
    //      InterpolationSpace (Linear->Device) : CS (L->D)
    //                |
    //             Merge (L)
    //              |     |
    //              |    CS (D->L)
    //              |          |
    //              |      Blend (D)
    //              |       /    |
    //              |  CS (L->D) |
    //              |  /         |
    //             Blur (L)      |
    //                 \         |
    //               CS (D->L)   |
    //                   \       |
    //                 Source Graphic (D)

    // Should be CS : InterpolationSpace (Linear->Device)
    EXPECT_EQ(filter->type(), PaintFilter::Type::kColorFilter);

    // Should be Merge.
    const auto* merge_effect_pf =
        static_cast<const ColorFilterPaintFilter*>(filter.get())->input().get();
    ASSERT_EQ(merge_effect_pf->type(), PaintFilter::Type::kMerge);
    const auto* merge = static_cast<const MergePaintFilter*>(merge_effect_pf);
    EXPECT_EQ(merge->input_count(), 2u);

    // Should be CS (D->L)
    const auto* color_filter_pf = merge->input_at(1u);
    ASSERT_EQ(color_filter_pf->type(), PaintFilter::Type::kColorFilter);

    // Should be Blend
    const auto* xfermode_filter_pf =
        static_cast<const ColorFilterPaintFilter*>(color_filter_pf)
            ->input()
            .get();
    ASSERT_TRUE(xfermode_filter_pf);
    EXPECT_EQ(xfermode_filter_pf->type(), PaintFilter::Type::kXfermode);
    const auto* xfermode =
        static_cast<const XfermodePaintFilter*>(xfermode_filter_pf);
    ASSERT_TRUE(xfermode->background());

    // Should be CS (L->D)
    color_filter_pf = xfermode->background().get();
    ASSERT_EQ(color_filter_pf->type(), PaintFilter::Type::kColorFilter);

    // Should be Blur
    const auto* blur_filter_pf =
        static_cast<const ColorFilterPaintFilter*>(color_filter_pf)
            ->input()
            .get();
    ASSERT_TRUE(blur_filter_pf);
    EXPECT_EQ(blur_filter_pf->type(), PaintFilter::Type::kBlur);

    // Should be CS (D->L)
    color_filter_pf =
        static_cast<const BlurPaintFilter*>(blur_filter_pf)->input().get();
    ASSERT_TRUE(color_filter_pf);
    EXPECT_EQ(color_filter_pf->type(), PaintFilter::Type::kColorFilter);
  }
};

TEST_F(ImageFilterBuilderTest, testInterpolationSpace) {
  InterpolationSpaceTest();
}

}  // namespace blink
