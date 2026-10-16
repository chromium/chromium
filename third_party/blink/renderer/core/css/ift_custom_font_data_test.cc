// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/ift_custom_font_data.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/font_face_cache.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/fonts/custom_font_data.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

// Table-keyed patch for 'b' applied to the base font.
constexpr char kPatchBTableUrl[] = "https://example.com/fonts/0K.ift_tk";
// Table-keyed patch for 'c' applied after `kPatchBTableUrl`.
constexpr char kPatchCAfterBTableUrl[] = "https://example.com/fonts/0C.ift_tk";
// Glyph-keyed patch with the outline for 'b'.
constexpr char kPatchBGlyphUrl[] = "https://example.com/fonts/04.1.ift_gk";
// Glyph-keyed patch with the outline for 'c'.
constexpr char kPatchCGlyphUrl[] = "https://example.com/fonts/08.1.ift_gk";

Vector<char> LoadTestData(const String& file_name) {
  std::optional<Vector<char>> data =
      test::ReadFromFile(test::PlatformTestDataPath(file_name));
  CHECK(data.has_value());
  return *data;
}

// Returns the test data for the patch at `url`.
Vector<char> PatchPayload(const String& url) {
  const KURL kurl(url);
  return LoadTestData(kurl.LastPathComponent().ToString());
}

class IftCustomFontDataTest : public SimTest {
 protected:
  void SetUp() override {
    SimTest::SetUp();
    scoped_feature_ =
        std::make_unique<ScopedIncrementalFontTransferForTest>(true);
  }

  // Loads a document with an IFT `@font-face` and returns the resulting
  // `IftCustomFontData`. `text` is set as the content of the element styled
  // with the font.
  //
  // The font is `roboto-ift.ttf`, a Roboto subset whose base font contains
  // only codepoint 'a'. Codepoints 'b' and 'c' are added through table-keyed
  // and glyph-keyed patches.
  const IftCustomFontData* LoadCustomFontData(const String& text = "a") {
    SimRequest main_resource("https://example.com/index.html", "text/html");
    SimSubresourceRequest font_resource(
        "https://example.com/fonts/roboto-ift.ttf", "font/ttf");

    LoadURL("https://example.com/index.html");
    main_resource.Complete(R"HTML(
      <!doctype html>
      <style>
        @font-face {
          font-family: ift-font;
          src: url(https://example.com/fonts/roboto-ift.ttf);
        }
        #target {
          font-family: ift-font;
        }
      </style>
      <span id="target"></span>
    )HTML");

    Compositor().BeginFrame();

    const auto& faces = GetDocument()
                            .GetStyleEngine()
                            .GetFontSelector()
                            ->GetFontFaceCache()
                            ->CssConnectedFontFaces();
    CHECK(!faces.empty());
    FontFace* font_face = faces.begin()->Get();
    CSSFontFace* css_font_face = font_face->CssFontFace();
    if (!css_font_face->IsValid()) {
      return nullptr;
    }
    css_font_face->Load();

    std::optional<Vector<char>> font_bytes =
        test::ReadFromFile(test::PlatformTestDataPath("roboto-ift.ttf"));
    CHECK(font_bytes.has_value());
    font_resource.Complete(*font_bytes);
    test::RunPendingTasks();

    Element* target = GetElementById("target");
    CHECK(target);
    target->setTextContent(text);
    const LayoutObject* layout_object = target->GetLayoutObject();
    CHECK(layout_object);

    const SimpleFontData* font_data =
        css_font_face->GetFontData(layout_object->Style().GetFontDescription());
    initial_font_data_ = font_data;
    return font_data ? static_cast<const IftCustomFontData*>(
                           font_data->GetCustomFontData())
                     : nullptr;
  }

  // Returns true if all of `#target` was shaped with `custom_font_data`.
  bool IsShapedWith(const IftCustomFontData* custom_font_data) {
    HeapVector<ShapeResult::RunFontData> run_fonts = TargetRunFonts();
    return !run_fonts.empty() &&
           std::ranges::all_of(
               run_fonts, [&](const ShapeResult::RunFontData& run) {
                 return run.font_data_->GetCustomFontData() == custom_font_data;
               });
  }

  // Returns true if all of `#target` was shaped with `custom_font_data` using
  // font data other than `initial_font_data_`, i.e. the font data rebuilt after
  // patches were applied.
  bool IsShapedWithUpdated(const IftCustomFontData* custom_font_data) {
    HeapVector<ShapeResult::RunFontData> run_fonts = TargetRunFonts();
    return !run_fonts.empty() &&
           std::ranges::all_of(run_fonts,
                               [&](const ShapeResult::RunFontData& run) {
                                 return run.font_data_->GetCustomFontData() ==
                                            custom_font_data &&
                                        run.font_data_ != initial_font_data_;
                               });
  }

  // The font data returned when the font was first loaded by
  // `LoadCustomFontData()`.
  Persistent<const SimpleFontData> initial_font_data_;

  // Returns the font data used to shape all of `#target`, or nullptr if
  // `#target` was shaped with more than one font data.
  const SimpleFontData* TargetFontData() {
    HeapVector<ShapeResult::RunFontData> run_fonts = TargetRunFonts();
    if (run_fonts.empty()) {
      return nullptr;
    }
    const SimpleFontData* font_data = run_fonts.front().font_data_;
    for (const ShapeResult::RunFontData& run : run_fonts) {
      if (run.font_data_ != font_data) {
        return nullptr;
      }
    }
    return font_data;
  }

 private:
  // Returns the font of each glyph run in the shaped text of `#target`.
  HeapVector<ShapeResult::RunFontData> TargetRunFonts() {
    Element* target = GetDocument().getElementById(AtomicString("target"));
    CHECK(target);
    const auto* layout_text = To<Text>(target->firstChild())->GetLayoutObject();
    CHECK(layout_text);
    const ShapeResult* shape_result =
        layout_text->InlineItems().front().TextShapeResult();
    CHECK(shape_result);
    HeapVector<ShapeResult::RunFontData> run_fonts;
    shape_result->GetRunFontData(&run_fonts);
    return run_fonts;
  }

  std::unique_ptr<ScopedIncrementalFontTransferForTest> scoped_feature_;
};

TEST_F(IftCustomFontDataTest, EmptyTextDoesNotRequireFetch) {
  const IftCustomFontData* custom_font_data = LoadCustomFontData();
  ASSERT_NE(custom_font_data, nullptr);

  EXPECT_TRUE(custom_font_data->IftRequireSubset(""));

  // No work was scheduled, so the font is still usable.
  test::RunPendingTasks();
  EXPECT_TRUE(custom_font_data->IftRequireSubset(""));
}

TEST_F(IftCustomFontDataTest, NewCodepointsTriggersCollection) {
  const IftCustomFontData* custom_font_data = LoadCustomFontData();
  ASSERT_NE(custom_font_data, nullptr);

  EXPECT_FALSE(custom_font_data->IftRequireSubset("a"));
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kCollecting);
}

TEST_F(IftCustomFontDataTest, CoveredCodepointsAreShapedWithFont) {
  const IftCustomFontData* custom_font_data = LoadCustomFontData("a");
  ASSERT_NE(custom_font_data, nullptr);

  // The first time a subset is seen, the font is not used yet.
  // TODO(wmedrano): Update once `IftRequireSubset()` checks the actual font,
  // at which point "a" is supported immediately.
  Compositor().BeginFrame();
  EXPECT_FALSE(IsShapedWith(custom_font_data));

  // Once the font is idle, it is invalidated and the text is shaped with it. No
  // patches were needed, so the initial font data is reused.
  test::RunPendingTasks();
  Compositor().BeginFrame();
  EXPECT_TRUE(IsShapedWith(custom_font_data));
  EXPECT_FALSE(IsShapedWithUpdated(custom_font_data));
}

TEST_F(IftCustomFontDataTest, MissingCodepointsAppliesPatch) {
  SimSubresourceRequest patch_b_table(kPatchBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_b_glyph(kPatchBGlyphUrl,
                                      "application/octet-stream");
  const IftCustomFontData* custom_font_data = LoadCustomFontData("b");
  ASSERT_NE(custom_font_data, nullptr);
  const Vector<uint8_t> original_font(
      custom_font_data->GetFontDataForTesting());

  // Text is shaped with a fallback font until the patches are applied.
  Compositor().BeginFrame();
  EXPECT_FALSE(IsShapedWith(custom_font_data));

  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFetching);

  // Once the patches are applied, the text is shaped with the patched font.
  patch_b_table.Complete(PatchPayload(kPatchBTableUrl));
  patch_b_glyph.Complete(PatchPayload(kPatchBGlyphUrl));
  test::RunPendingTasks();
  EXPECT_NE(custom_font_data->GetFontDataForTesting(),
            base::span(original_font));
  Compositor().BeginFrame();
  EXPECT_TRUE(IsShapedWithUpdated(custom_font_data));
  EXPECT_TRUE(custom_font_data->IftRequireSubset("b"));
}

TEST_F(IftCustomFontDataTest, AppliesPatchesAfterAllPatchesFetched) {
  SimSubresourceRequest patch_b_table(kPatchBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_b_glyph(kPatchBGlyphUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_c_glyph(kPatchCGlyphUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_c_table(kPatchCAfterBTableUrl,
                                      "application/octet-stream");

  // Load with text that requires both "b" and "c" patches.
  const IftCustomFontData* custom_font_data = LoadCustomFontData("bc");
  ASSERT_NE(custom_font_data, nullptr);
  const Vector<uint8_t> original_font(
      custom_font_data->GetFontDataForTesting());

  // Completing "b" patch will still wait on "c" patch.
  Compositor().BeginFrame();
  test::RunPendingTasks();
  patch_b_glyph.Complete(PatchPayload(kPatchBGlyphUrl));
  patch_b_table.Complete(PatchPayload(kPatchBTableUrl));
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFetching);
  EXPECT_EQ(custom_font_data->GetFontDataForTesting(),
            base::span(original_font));
  EXPECT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_FALSE(IsShapedWith(custom_font_data));

  // Completing "c" patch applies patch and the text is shaped with it.
  patch_c_glyph.Complete(PatchPayload(kPatchCGlyphUrl));
  patch_c_table.Complete(PatchPayload(kPatchCAfterBTableUrl));
  test::RunPendingTasks();
  EXPECT_NE(custom_font_data->GetFontDataForTesting(),
            base::span(original_font));
  Compositor().BeginFrame();
  EXPECT_TRUE(IsShapedWithUpdated(custom_font_data));
  EXPECT_TRUE(custom_font_data->IftRequireSubset("bc"));
}

TEST_F(IftCustomFontDataTest, CodepointsRequestedWhileFetchingAreFetched) {
  SimSubresourceRequest patch_b_table(kPatchBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_b_glyph(kPatchBGlyphUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_c_table(kPatchCAfterBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_c_glyph(kPatchCGlyphUrl,
                                      "application/octet-stream");

  // Start initial fetch for "b".
  const IftCustomFontData* custom_font_data = LoadCustomFontData("b");
  ASSERT_NE(custom_font_data, nullptr);
  const Vector<uint8_t> original_font(
      custom_font_data->GetFontDataForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFetching);

  // Request support for "c" by adding it to the text.
  GetDocument()
      .getElementById(AtomicString("target"))
      ->setTextContent(String("bc"));
  Compositor().BeginFrame();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFetching);

  // Patches for "b" are applied even though we still need to wait for "c". The
  // patched font is not rendered until all patches are applied.
  patch_b_table.Complete(PatchPayload(kPatchBTableUrl));
  patch_b_glyph.Complete(PatchPayload(kPatchBGlyphUrl));
  test::RunPendingTasks();
  const Vector<uint8_t> font_data_b(custom_font_data->GetFontDataForTesting());
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFetching);
  EXPECT_NE(font_data_b, original_font);
  EXPECT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_FALSE(IsShapedWithUpdated(custom_font_data));

  // Patches for "c" are applied and the text is shaped with the patched font.
  patch_c_table.Complete(PatchPayload(kPatchCAfterBTableUrl));
  patch_c_glyph.Complete(PatchPayload(kPatchCGlyphUrl));
  test::RunPendingTasks();
  EXPECT_NE(custom_font_data->GetFontDataForTesting(), base::span(font_data_b));
  Compositor().BeginFrame();
  EXPECT_TRUE(IsShapedWithUpdated(custom_font_data));
  EXPECT_TRUE(custom_font_data->IftRequireSubset("bc"));
}

TEST_F(IftCustomFontDataTest, InvalidPatchDataFails) {
  SimSubresourceRequest patch_b_table(kPatchBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_b_glyph(kPatchBGlyphUrl,
                                      "application/octet-stream");
  const IftCustomFontData* custom_font_data = LoadCustomFontData("b");
  ASSERT_NE(custom_font_data, nullptr);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFetching);

  patch_b_table.Complete(PatchPayload(kPatchBTableUrl));
  patch_b_glyph.Complete("not-a-patch");
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFailed);

  // A failed font is never used.
  EXPECT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_FALSE(IsShapedWith(custom_font_data));
}

TEST_F(IftCustomFontDataTest, PatchFetchFailureFails) {
  SimRequestBase::Params not_found;
  not_found.response_http_status = 404;
  SimSubresourceRequest patch_b_table(kPatchBTableUrl,
                                      "application/octet-stream", not_found);
  SimSubresourceRequest patch_b_glyph(kPatchBGlyphUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_c_glyph(kPatchCGlyphUrl,
                                      "application/octet-stream");
  const IftCustomFontData* custom_font_data = LoadCustomFontData("bc");
  ASSERT_NE(custom_font_data, nullptr);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFetching);

  // A failed patch fails the font, even if other patches succeed.
  patch_b_table.Complete();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFailed);

  patch_b_glyph.Complete(PatchPayload(kPatchBGlyphUrl));
  patch_c_glyph.Complete(PatchPayload(kPatchCGlyphUrl));
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFailed);
}

TEST_F(IftCustomFontDataTest, SecondPatchRoundIsRendered) {
  SimSubresourceRequest patch_b_table(kPatchBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_b_glyph(kPatchBGlyphUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_c_table(kPatchCAfterBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_c_glyph(kPatchCGlyphUrl,
                                      "application/octet-stream");

  // First round: patches for "b" are applied and rendered.
  const IftCustomFontData* custom_font_data = LoadCustomFontData("b");
  ASSERT_NE(custom_font_data, nullptr);
  Compositor().BeginFrame();
  test::RunPendingTasks();
  patch_b_table.Complete(PatchPayload(kPatchBTableUrl));
  patch_b_glyph.Complete(PatchPayload(kPatchBGlyphUrl));
  test::RunPendingTasks();
  Compositor().BeginFrame();
  ASSERT_TRUE(IsShapedWithUpdated(custom_font_data));
  Persistent<const SimpleFontData> font_data_b = TargetFontData();
  ASSERT_NE(font_data_b.Get(), nullptr);

  // Second round: requesting "c" fetches and renders patches again.
  GetDocument()
      .getElementById(AtomicString("target"))
      ->setTextContent(String("bc"));
  Compositor().BeginFrame();
  test::RunPendingTasks();
  patch_c_table.Complete(PatchPayload(kPatchCAfterBTableUrl));
  patch_c_glyph.Complete(PatchPayload(kPatchCGlyphUrl));
  test::RunPendingTasks();
  Compositor().BeginFrame();
  EXPECT_TRUE(IsShapedWithUpdated(custom_font_data));
  const SimpleFontData* font_data_bc = TargetFontData();
  ASSERT_NE(font_data_bc, nullptr);
  EXPECT_NE(font_data_bc, font_data_b.Get());
}

TEST_F(IftCustomFontDataTest, DocumentDetachedBeforeCollectingFails) {
  const IftCustomFontData* custom_font_data = LoadCustomFontData();
  ASSERT_NE(custom_font_data, nullptr);

  GetDocument().Shutdown();
  EXPECT_FALSE(custom_font_data->IftRequireSubset("a"));
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFailed);

  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFailed);
}

TEST_F(IftCustomFontDataTest, DocumentDetachedBeforeFetchingFails) {
  const IftCustomFontData* custom_font_data = LoadCustomFontData("b");
  ASSERT_NE(custom_font_data, nullptr);

  Compositor().BeginFrame();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kCollecting);

  GetDocument().Shutdown();
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFailed);
}

}  // namespace
}  // namespace blink
