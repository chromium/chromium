// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/ift_custom_font_data.h"

#include <memory>
#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/css/css_font_face_source.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/font_face_cache.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/fonts/custom_font_data.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

// Patch URLs for codepoints 'b' and 'c' in `roboto-ift.ttf`, resolved against
// the font URL.
constexpr char kPatchBUrl[] = "https://example.com/fonts/00.1.ift_gk";
constexpr char kPatchCUrl[] = "https://example.com/fonts/04.1.ift_gk";

Vector<char> LoadTestData(const String& file_name) {
  std::optional<Vector<char>> data =
      test::ReadFromFile(test::PlatformTestDataPath(file_name));
  CHECK(data.has_value());
  return *data;
}

// Glyph-keyed patch for codepoint 'b'.
Vector<char> PatchBPayload() {
  return LoadTestData("00.1.ift_gk");
}

// Glyph-keyed patch for codepoint 'c'.
Vector<char> PatchCPayload() {
  return LoadTestData("04.1.ift_gk");
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
  // only codepoint 'a'. Its IFT tables map codepoints 'b' and 'c' to
  // glyph-keyed patches (`00.1.ift_gk` and `04.1.ift_gk`, respectively). See
  // `third_party/blink/renderer/platform/testing/data/third_party/RobotoClassic/README.chromium`.
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
    auto* source =
        const_cast<CSSFontFaceSource*>(font_face->CssFontFace()->FrontSource());
    if (!source) {
      return nullptr;
    }
    source->BeginLoadIfNeeded();

    std::optional<Vector<char>> font_bytes =
        test::ReadFromFile(test::PlatformTestDataPath("roboto-ift.ttf"));
    CHECK(font_bytes.has_value());
    font_resource.Complete(*font_bytes);
    test::RunPendingTasks();

    GetElementById("target")->setTextContent(text);

    const SimpleFontData* font_data =
        source->GetFontData(FontDescription(), FontSelectionCapabilities());
    return font_data ? static_cast<const IftCustomFontData*>(
                           font_data->GetCustomFontData())
                     : nullptr;
  }

 private:
  std::unique_ptr<ScopedIncrementalFontTransferForTest> scoped_feature_;
};

TEST_F(IftCustomFontDataTest, NoNewSubsetIsIdle) {
  const IftCustomFontData* custom_font_data = LoadCustomFontData();
  ASSERT_NE(custom_font_data, nullptr);
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kIdle);

  EXPECT_TRUE(custom_font_data->IftRequireSubset(""));
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kIdle);

  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kIdle);
}

TEST_F(IftCustomFontDataTest, NewCodepointsTriggersCollection) {
  const IftCustomFontData* custom_font_data = LoadCustomFontData();
  ASSERT_NE(custom_font_data, nullptr);

  EXPECT_FALSE(custom_font_data->IftRequireSubset("a"));
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kCollecting);
}

TEST_F(IftCustomFontDataTest, CoveredCodepointsTransitionToIdle) {
  const IftCustomFontData* custom_font_data = LoadCustomFontData("a");
  ASSERT_NE(custom_font_data, nullptr);

  Compositor().BeginFrame();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kCollecting);

  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kIdle);
}

TEST_F(IftCustomFontDataTest, MissingCodepointsAppliesPatch) {
  SimSubresourceRequest patch_b(kPatchBUrl, "application/octet-stream");
  const IftCustomFontData* custom_font_data = LoadCustomFontData("b");
  ASSERT_NE(custom_font_data, nullptr);
  const Vector<uint8_t> original_font(
      custom_font_data->GetFontDataForTesting());

  Compositor().BeginFrame();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kCollecting);

  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFetching);

  patch_b.Complete(PatchBPayload());
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kIdle);
  EXPECT_NE(custom_font_data->GetFontDataForTesting(),
            base::span(original_font));
  EXPECT_TRUE(custom_font_data->IftRequireSubset("b"));
}

TEST_F(IftCustomFontDataTest, AppliesPatchesAfterAllPatchesFetched) {
  SimSubresourceRequest patch_b(kPatchBUrl, "application/octet-stream");
  SimSubresourceRequest patch_c(kPatchCUrl, "application/octet-stream");

  // Load with text that requires both "b" and "c" patch.
  const IftCustomFontData* custom_font_data = LoadCustomFontData("bc");
  ASSERT_NE(custom_font_data, nullptr);
  const Vector<uint8_t> original_font(
      custom_font_data->GetFontDataForTesting());

  // Completing "b" patch will still wait on "c" patch.
  Compositor().BeginFrame();
  test::RunPendingTasks();
  patch_b.Complete(PatchBPayload());
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFetching);
  EXPECT_EQ(custom_font_data->GetFontDataForTesting(),
            base::span(original_font));

  // Completing "c" patch applies patch and goes back to idle.
  patch_c.Complete(PatchCPayload());
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kIdle);
  EXPECT_NE(custom_font_data->GetFontDataForTesting(),
            base::span(original_font));
  EXPECT_TRUE(custom_font_data->IftRequireSubset("bc"));
}

TEST_F(IftCustomFontDataTest, CodepointsRequestedWhileFetchingAreFetched) {
  SimSubresourceRequest patch_b(kPatchBUrl, "application/octet-stream");
  SimSubresourceRequest patch_c(kPatchCUrl, "application/octet-stream");

  // Start initial fetch for patch "b".
  const IftCustomFontData* custom_font_data = LoadCustomFontData("b");
  ASSERT_NE(custom_font_data, nullptr);
  const Vector<uint8_t> original_font(
      custom_font_data->GetFontDataForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFetching);

  // Request support for "c".
  EXPECT_FALSE(custom_font_data->IftRequireSubset("c"));
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFetching);

  // Patch for "b" is applied even though we still need to wait for "c".
  patch_b.Complete(PatchBPayload());
  test::RunPendingTasks();
  const Vector<uint8_t> font_data_b(custom_font_data->GetFontDataForTesting());
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFetching);
  EXPECT_NE(font_data_b, original_font);

  // Patch "c" is applied.
  patch_c.Complete(PatchCPayload());
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kIdle);
  EXPECT_NE(custom_font_data->GetFontDataForTesting(), base::span(font_data_b));
  EXPECT_TRUE(custom_font_data->IftRequireSubset("bc"));
}

TEST_F(IftCustomFontDataTest, InvalidPatchDataFails) {
  SimSubresourceRequest patch_b(kPatchBUrl, "application/octet-stream");
  const IftCustomFontData* custom_font_data = LoadCustomFontData("b");
  ASSERT_NE(custom_font_data, nullptr);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFetching);

  patch_b.Complete("not-a-patch");
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFailed);
}

TEST_F(IftCustomFontDataTest, PatchFetchFailureFails) {
  SimRequestBase::Params not_found;
  not_found.response_http_status = 404;
  SimSubresourceRequest patch_b(kPatchBUrl, "application/octet-stream",
                                not_found);
  SimSubresourceRequest patch_c(kPatchCUrl, "application/octet-stream");
  const IftCustomFontData* custom_font_data = LoadCustomFontData("bc");
  ASSERT_NE(custom_font_data, nullptr);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFetching);

  // A failed patch fails the font, even if other patches succeed.
  patch_b.Complete();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFailed);

  patch_c.Complete(PatchCPayload());
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFailed);
}

TEST_F(IftCustomFontDataTest, AllPatchFetchesFailingFails) {
  SimRequestBase::Params not_found;
  not_found.response_http_status = 404;
  SimSubresourceRequest patch_b(kPatchBUrl, "application/octet-stream",
                                not_found);
  SimSubresourceRequest patch_c(kPatchCUrl, "application/octet-stream",
                                not_found);
  const IftCustomFontData* custom_font_data = LoadCustomFontData("bc");
  ASSERT_NE(custom_font_data, nullptr);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  patch_b.Complete();
  patch_c.Complete();
  test::RunPendingTasks();
  EXPECT_EQ(custom_font_data->GetStateForTesting(),
            IftCustomFontData::State::kFailed);
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
