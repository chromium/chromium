// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/ift_custom_font_data.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/font_face_cache.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/loader/resource/ift_font.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/fonts/custom_font_data.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkFourByteTag.h"

namespace blink {
namespace {

// Table-keyed patch for 'b' applied to the base font.
constexpr char kPatchBTableUrl[] = "https://example.com/fonts/1K.ift_tk";
// Table-keyed patch for 'c' applied after `kPatchBTableUrl`.
constexpr char kPatchCAfterBTableUrl[] = "https://example.com/fonts/14.ift_tk";
// Glyph-keyed patch with the outline for 'b'.
constexpr char kPatchBGlyphUrl[] = "https://example.com/fonts/04.1.ift_gk";
// Glyph-keyed patch with the outline for 'b' at expanded wght [500, 900].
constexpr char kPatchBExpandedWghtGlyphUrl[] =
    "https://example.com/fonts/04.2.ift_gk";
// Glyph-keyed patch with the outline for 'c'.
constexpr char kPatchCGlyphUrl[] = "https://example.com/fonts/08.1.ift_gk";
// Table-keyed patch for wght [500, 900] applied to the base font.
constexpr char kPatchWghtTableUrl[] = "https://example.com/fonts/1S.ift_tk";
// Table-keyed patch for 'b' applied after `kPatchWghtTableUrl`.
constexpr char kPatchBAfterWghtTableUrl[] =
    "https://example.com/fonts/0O.ift_tk";

constexpr SkFourByteTag kGradTag = SkSetFourByteTag('G', 'R', 'A', 'D');
constexpr SkFourByteTag kItalTag = SkSetFourByteTag('i', 't', 'a', 'l');
constexpr SkFourByteTag kOpszTag = SkSetFourByteTag('o', 'p', 's', 'z');
constexpr SkFourByteTag kSlntTag = SkSetFourByteTag('s', 'l', 'n', 't');
constexpr SkFourByteTag kWdthTag = SkSetFourByteTag('w', 'd', 't', 'h');
constexpr SkFourByteTag kWghtTag = SkSetFourByteTag('w', 'g', 'h', 't');

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
  // `IftFont`. `text` is set as the content of the element styled with the
  // font.
  //
  // The font is `roboto-ift.ttf`, a Roboto subset whose base font contains
  // codepoint 'a', feature 'smcp' (GSUB table), and the wght [100, 400] design
  // space. Codepoints 'b' and 'c', 'smcp' outlines, and wght [500, 900] are
  // added through table-keyed and glyph-keyed patches.
  //
  // If `world` is set, the `@font-face` rule is parsed in `world`. If
  // `font_redirect_url` is set, the font request is redirected to it.
  // `font_face_descriptors_` are added to the `@font-face` rule.
  const IftFont* LoadIftFont(const String& text = "a",
                             DOMWrapperWorld* world = nullptr,
                             const String& font_redirect_url = String()) {
    SimRequest main_resource("https://example.com/index.html", "text/html");
    SimRequest::Params font_params;
    font_params.redirect_url = font_redirect_url;
    SimSubresourceRequest font_resource(
        "https://example.com/fonts/roboto-ift.ttf", "font/ttf", font_params);
    std::optional<SimSubresourceRequest> redirected_font_resource;
    if (!font_redirect_url.empty()) {
      redirected_font_resource.emplace(font_redirect_url, "font/ttf");
    }

    LoadURL("https://example.com/index.html");
    main_resource.Write("<!doctype html>");
    std::optional<ScriptState::Scope> world_scope;
    if (world) {
      world_scope.emplace(ToScriptState(GetDocument().GetFrame(), *world));
    }
    main_resource.Complete(StrCat({R"HTML(
      <style>
        @font-face {
          font-family: ift-font;
          src: url(https://example.com/fonts/roboto-ift.ttf);
          )HTML",
                                   font_face_descriptors_, R"HTML(
        }
        #target {
          font-family: ift-font;
        }
      </style>
      <span id="target"></span>
    )HTML"}));
    world_scope.reset();

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
    (redirected_font_resource ? *redirected_font_resource : font_resource)
        .Complete(*font_bytes);
    test::RunPendingTasks();

    Element* target = GetElementById("target");
    CHECK(target);
    target->setTextContent(text);
    const LayoutObject* layout_object = target->GetLayoutObject();
    CHECK(layout_object);

    const SimpleFontData* font_data =
        css_font_face->GetFontData(layout_object->Style().GetFontDescription());
    initial_font_data_ = font_data;
    return font_data ? GetIftFont(font_data) : nullptr;
  }

  // Returns the `IftCustomFontData` of `font_data`, or nullptr if `font_data`
  // is null or not a loaded custom font. The only `@font-face` in these tests
  // is an IFT font, so every loaded custom font created while
  // `IncrementalFontTransfer` is enabled is assumed to be backed by an
  // `IftCustomFontData` (do not call when `IncrementalFontTransfer` was
  // disabled at `CreateFontData` time).
  static const IftCustomFontData* GetIftCustomFontData(
      const SimpleFontData* font_data) {
    if (!font_data) {
      return nullptr;
    }
    const CustomFontData* custom_font_data = font_data->GetCustomFontData();
    if (!custom_font_data || custom_font_data->IsLoadingFallback()) {
      return nullptr;
    }
    return static_cast<const IftCustomFontData*>(custom_font_data);
  }

  // Returns the shared `IftFont` of `font_data`, or nullptr if it is not backed
  // by an `IftCustomFontData`.
  static const IftFont* GetIftFont(const SimpleFontData* font_data) {
    const IftCustomFontData* custom_font_data = GetIftCustomFontData(font_data);
    return custom_font_data ? custom_font_data->GetIftFont() : nullptr;
  }

  // Returns true if all of `#target` was shaped with `ift_font`.
  bool IsShapedWith(const IftFont* ift_font) {
    HeapVector<ShapeResult::RunFontData> run_fonts = TargetRunFonts();
    return !run_fonts.empty() &&
           std::ranges::all_of(run_fonts,
                               [&](const ShapeResult::RunFontData& run) {
                                 return GetIftFont(run.font_data_) == ift_font;
                               });
  }

  // Returns true if all of `#target` was shaped with `ift_font` using font data
  // other than `initial_font_data_`, i.e. the font data rebuilt after patches
  // were applied.
  bool IsShapedWithUpdated(const IftFont* ift_font) {
    HeapVector<ShapeResult::RunFontData> run_fonts = TargetRunFonts();
    return !run_fonts.empty() &&
           std::ranges::all_of(
               run_fonts, [&](const ShapeResult::RunFontData& run) {
                 return GetIftFont(run.font_data_) == ift_font &&
                        run.font_data_ != initial_font_data_;
               });
  }

  // The font data returned when the font was first loaded by `LoadIftFont()`.
  Persistent<const SimpleFontData> initial_font_data_;

  // Additional descriptors for the `@font-face` rule of `LoadIftFont()`, e.g.
  // "font-weight: 100 400;".
  String font_face_descriptors_;

  // Sets the inline style of `#target` to `style`.
  void SetTargetStyle(const char* style) {
    GetElementById("target")->setAttribute(html_names::kStyleAttr,
                                           AtomicString(style));
  }

  // Returns the font data used to shape all of `#id`, or nullptr if `#id` was
  // shaped with zero or more than one font data.
  const SimpleFontData* ElementFontData(const char* id) {
    HeapVector<ShapeResult::RunFontData> run_fonts = ElementRunFonts(id);
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

  const SimpleFontData* TargetFontData() { return ElementFontData("target"); }

  // Returns the font of each glyph run in the shaped text of `#id`.
  HeapVector<ShapeResult::RunFontData> ElementRunFonts(const char* id) {
    Element* element = GetElementById(id);
    CHECK(element);
    const auto* layout_text =
        To<Text>(element->firstChild())->GetLayoutObject();
    CHECK(layout_text);
    const ShapeResult* shape_result =
        layout_text->InlineItems().front().TextShapeResult();
    CHECK(shape_result);
    HeapVector<ShapeResult::RunFontData> run_fonts;
    shape_result->GetRunFontData(&run_fonts);
    return run_fonts;
  }

  HeapVector<ShapeResult::RunFontData> TargetRunFonts() {
    return ElementRunFonts("target");
  }

 private:
  std::unique_ptr<ScopedIncrementalFontTransferForTest> scoped_feature_;
};

TEST_F(IftCustomFontDataTest, SupportedTextDoesNotRequireFetch) {
  const IftFont* ift_font = LoadIftFont();
  ASSERT_NE(ift_font, nullptr);

  EXPECT_TRUE(ift_font->RequireSubset(""));
  EXPECT_TRUE(ift_font->RequireSubset("a"));

  // No work was scheduled, so the font is still usable.
  test::RunPendingTasks();
  EXPECT_TRUE(ift_font->RequireSubset(""));
  EXPECT_TRUE(ift_font->RequireSubset("a"));
}

TEST_F(IftCustomFontDataTest, NewUnsupportedCodepointsTriggersCollection) {
  SimSubresourceRequest patch_b_table(kPatchBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_b_glyph(kPatchBGlyphUrl,
                                      "application/octet-stream");
  const IftFont* ift_font = LoadIftFont();
  ASSERT_NE(ift_font, nullptr);

  EXPECT_FALSE(ift_font->RequireSubset("b"));
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kCollecting);

  test::RunPendingTasks();
  patch_b_table.Complete(PatchPayload(kPatchBTableUrl));
  patch_b_glyph.Complete(PatchPayload(kPatchBGlyphUrl));
}

TEST_F(IftCustomFontDataTest, CoveredCodepointsAreShapedWithFont) {
  const IftFont* ift_font = LoadIftFont("a");
  ASSERT_NE(ift_font, nullptr);

  // Codepoint 'a' is already covered by the base font, so it is shaped with it
  // immediately without triggering collection.
  Compositor().BeginFrame();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kIdle);
  EXPECT_TRUE(IsShapedWith(ift_font));
  EXPECT_FALSE(IsShapedWithUpdated(ift_font));
}

TEST_F(IftCustomFontDataTest,
       UnpatchableCodepointsFallBackWithoutBlockingSupportedText) {
  // 'a' is in the base font; 'z' is neither in the base font nor in any patch.
  const IftFont* ift_font = LoadIftFont("az");
  ASSERT_NE(ift_font, nullptr);

  Compositor().BeginFrame();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kIdle);
  HeapVector<ShapeResult::RunFontData> run_fonts = TargetRunFonts();
  ASSERT_EQ(run_fonts.size(), 2u);
  EXPECT_EQ(GetIftFont(run_fonts[0].font_data_), ift_font);
  EXPECT_NE(GetIftFont(run_fonts[1].font_data_), ift_font);
}

TEST_F(IftCustomFontDataTest, MissingCodepointsAppliesPatch) {
  SimSubresourceRequest patch_b_table(kPatchBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_b_glyph(kPatchBGlyphUrl,
                                      "application/octet-stream");
  const IftFont* ift_font = LoadIftFont("b");
  ASSERT_NE(ift_font, nullptr);
  const Vector<uint8_t> original_font(ift_font->GetFontDataForTesting());

  // Text is shaped with a fallback font until the patches are applied.
  Compositor().BeginFrame();
  EXPECT_FALSE(IsShapedWith(ift_font));

  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFetching);

  // Once the patches are applied, the text is shaped with the patched font.
  patch_b_table.Complete(PatchPayload(kPatchBTableUrl));
  patch_b_glyph.Complete(PatchPayload(kPatchBGlyphUrl));
  test::RunPendingTasks();
  EXPECT_NE(ift_font->GetFontDataForTesting(), base::span(original_font));
  Compositor().BeginFrame();
  EXPECT_TRUE(IsShapedWithUpdated(ift_font));
  EXPECT_TRUE(ift_font->RequireSubset("b"));
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
  const IftFont* ift_font = LoadIftFont("bc");
  ASSERT_NE(ift_font, nullptr);
  const Vector<uint8_t> original_font(ift_font->GetFontDataForTesting());

  // Completing "b" patch will still wait on "c" patch.
  Compositor().BeginFrame();
  test::RunPendingTasks();
  patch_b_glyph.Complete(PatchPayload(kPatchBGlyphUrl));
  patch_b_table.Complete(PatchPayload(kPatchBTableUrl));
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFetching);
  EXPECT_EQ(ift_font->GetFontDataForTesting(), base::span(original_font));
  EXPECT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_FALSE(IsShapedWith(ift_font));

  // Completing "c" patch applies patch and the text is shaped with it.
  patch_c_glyph.Complete(PatchPayload(kPatchCGlyphUrl));
  patch_c_table.Complete(PatchPayload(kPatchCAfterBTableUrl));
  test::RunPendingTasks();
  EXPECT_NE(ift_font->GetFontDataForTesting(), base::span(original_font));
  Compositor().BeginFrame();
  EXPECT_TRUE(IsShapedWithUpdated(ift_font));
  EXPECT_TRUE(ift_font->RequireSubset("bc"));
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
  const IftFont* ift_font = LoadIftFont("b");
  ASSERT_NE(ift_font, nullptr);
  const Vector<uint8_t> original_font(ift_font->GetFontDataForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFetching);

  // Request support for "c" by adding it to the text.
  GetDocument()
      .getElementById(AtomicString("target"))
      ->setTextContent(String("bc"));
  Compositor().BeginFrame();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFetching);

  // Patches for "b" are applied even though we still need to wait for "c". The
  // patched font is not rendered until all patches are applied.
  patch_b_table.Complete(PatchPayload(kPatchBTableUrl));
  patch_b_glyph.Complete(PatchPayload(kPatchBGlyphUrl));
  test::RunPendingTasks();
  const Vector<uint8_t> font_data_b(ift_font->GetFontDataForTesting());
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFetching);
  EXPECT_NE(font_data_b, original_font);
  EXPECT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_FALSE(IsShapedWithUpdated(ift_font));
  // "b" is patched but not rendered yet, so the font is not ready for it.
  // Text the rendered font already supports is unaffected.
  EXPECT_FALSE(ift_font->RequireSubset("b"));
  EXPECT_TRUE(ift_font->RequireSubset("a"));
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFetching);

  // Patches for "c" are applied and the text is shaped with the patched font.
  patch_c_table.Complete(PatchPayload(kPatchCAfterBTableUrl));
  patch_c_glyph.Complete(PatchPayload(kPatchCGlyphUrl));
  test::RunPendingTasks();
  EXPECT_NE(ift_font->GetFontDataForTesting(), base::span(font_data_b));
  Compositor().BeginFrame();
  EXPECT_TRUE(IsShapedWithUpdated(ift_font));
  EXPECT_TRUE(ift_font->RequireSubset("bc"));
}

TEST_F(IftCustomFontDataTest, InvalidPatchDataFails) {
  SimSubresourceRequest patch_b_table(kPatchBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_b_glyph(kPatchBGlyphUrl,
                                      "application/octet-stream");
  const IftFont* ift_font = LoadIftFont("b");
  ASSERT_NE(ift_font, nullptr);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFetching);

  patch_b_table.Complete(PatchPayload(kPatchBTableUrl));
  patch_b_glyph.Complete("not-a-patch");
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFailed);

  // A failed font is never used.
  EXPECT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_FALSE(IsShapedWith(ift_font));
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
  const IftFont* ift_font = LoadIftFont("bc");
  ASSERT_NE(ift_font, nullptr);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFetching);

  // A failed patch fails the font, even if other patches succeed.
  patch_b_table.Complete();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFailed);

  patch_b_glyph.Complete(PatchPayload(kPatchBGlyphUrl));
  patch_c_glyph.Complete(PatchPayload(kPatchCGlyphUrl));
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFailed);
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
  const IftFont* ift_font = LoadIftFont("b");
  ASSERT_NE(ift_font, nullptr);
  Compositor().BeginFrame();
  test::RunPendingTasks();
  patch_b_table.Complete(PatchPayload(kPatchBTableUrl));
  patch_b_glyph.Complete(PatchPayload(kPatchBGlyphUrl));
  test::RunPendingTasks();
  Compositor().BeginFrame();
  ASSERT_TRUE(IsShapedWithUpdated(ift_font));
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
  EXPECT_TRUE(IsShapedWithUpdated(ift_font));
  const SimpleFontData* font_data_bc = TargetFontData();
  ASSERT_NE(font_data_bc, nullptr);
  EXPECT_NE(font_data_bc, font_data_b.Get());
}

TEST_F(IftCustomFontDataTest, ExceedingTotalPatchLimitFails) {
  const IftFont* ift_font = LoadIftFont("b");
  ASSERT_NE(ift_font, nullptr);
  // 'b' requires two patches.
  ift_font->SetPatchLimitsForTesting(/*max_invalidating_patches=*/100,
                                     /*max_total_patches=*/1);

  // The font fails before fetching any patch and is never used.
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFailed);
  EXPECT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_FALSE(IsShapedWith(ift_font));
}

TEST_F(IftCustomFontDataTest, ExceedingInvalidatingPatchLimitFails) {
  SimSubresourceRequest patch_b_table(kPatchBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_b_glyph(kPatchBGlyphUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_c_glyph(kPatchCGlyphUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_c_table(kPatchCAfterBTableUrl,
                                      "application/octet-stream");
  const IftFont* ift_font = LoadIftFont("bc");
  ASSERT_NE(ift_font, nullptr);
  // "bc" requires two invalidating patches.
  ift_font->SetPatchLimitsForTesting(/*max_invalidating_patches=*/1,
                                     /*max_total_patches=*/2000);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  patch_b_table.Complete(PatchPayload(kPatchBTableUrl));
  patch_b_glyph.Complete(PatchPayload(kPatchBGlyphUrl));
  patch_c_glyph.Complete(PatchPayload(kPatchCGlyphUrl));
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFetching);

  // Applying the second invalidating patch exceeds the limit.
  patch_c_table.Complete(PatchPayload(kPatchCAfterBTableUrl));
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFailed);
  EXPECT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_FALSE(IsShapedWith(ift_font));
}

TEST_F(IftCustomFontDataTest, PatchLimitsResetForEachExtension) {
  SimSubresourceRequest patch_b_table(kPatchBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_b_glyph(kPatchBGlyphUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_c_table(kPatchCAfterBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_c_glyph(kPatchCGlyphUrl,
                                      "application/octet-stream");
  const IftFont* ift_font = LoadIftFont("b");
  ASSERT_NE(ift_font, nullptr);
  // "b" and "c" each require one invalidating patch.
  ift_font->SetPatchLimitsForTesting(/*max_invalidating_patches=*/1,
                                     /*max_total_patches=*/2000);

  // First extension: "b".
  Compositor().BeginFrame();
  test::RunPendingTasks();
  patch_b_table.Complete(PatchPayload(kPatchBTableUrl));
  patch_b_glyph.Complete(PatchPayload(kPatchBGlyphUrl));
  test::RunPendingTasks();
  ASSERT_EQ(ift_font->GetStateForTesting(), IftFont::State::kIdle);
  Compositor().BeginFrame();
  ASSERT_TRUE(IsShapedWithUpdated(ift_font));

  // Second extension: "c" gets its own patch limits.
  GetDocument()
      .getElementById(AtomicString("target"))
      ->setTextContent(String("bc"));
  Compositor().BeginFrame();
  test::RunPendingTasks();
  patch_c_table.Complete(PatchPayload(kPatchCAfterBTableUrl));
  patch_c_glyph.Complete(PatchPayload(kPatchCGlyphUrl));
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kIdle);
  Compositor().BeginFrame();
  EXPECT_TRUE(IsShapedWithUpdated(ift_font));
  EXPECT_TRUE(ift_font->RequireSubset("bc"));
}

TEST_F(IftCustomFontDataTest, TextAddedWhileFetchingSharesPatchLimits) {
  SimSubresourceRequest patch_b_table(kPatchBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_b_glyph(kPatchBGlyphUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_c_table(kPatchCAfterBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_c_glyph(kPatchCGlyphUrl,
                                      "application/octet-stream");
  const IftFont* ift_font = LoadIftFont("b");
  ASSERT_NE(ift_font, nullptr);
  // "b" and "c" each require one invalidating patch.
  ift_font->SetPatchLimitsForTesting(/*max_invalidating_patches=*/1,
                                     /*max_total_patches=*/2000);
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFetching);

  // "c" is requested while "b" is fetching, so it joins the same extension.
  GetDocument()
      .getElementById(AtomicString("target"))
      ->setTextContent(String("bc"));
  Compositor().BeginFrame();
  patch_b_table.Complete(PatchPayload(kPatchBTableUrl));
  patch_b_glyph.Complete(PatchPayload(kPatchBGlyphUrl));
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFetching);

  // Applying the second invalidating patch exceeds the limit.
  patch_c_table.Complete(PatchPayload(kPatchCAfterBTableUrl));
  patch_c_glyph.Complete(PatchPayload(kPatchCGlyphUrl));
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFailed);
  EXPECT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_FALSE(IsShapedWithUpdated(ift_font));
}

TEST_F(IftCustomFontDataTest, DocumentDetachedBeforeCollectingFails) {
  const IftFont* ift_font = LoadIftFont();
  ASSERT_NE(ift_font, nullptr);

  GetDocument().Shutdown();
  EXPECT_FALSE(ift_font->RequireSubset("b"));
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFailed);

  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFailed);
}

TEST_F(IftCustomFontDataTest, DocumentDetachedBeforeFetchingFails) {
  const IftFont* ift_font = LoadIftFont("b");
  ASSERT_NE(ift_font, nullptr);

  Compositor().BeginFrame();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kCollecting);

  GetDocument().Shutdown();
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFailed);
}

TEST_F(IftCustomFontDataTest, PatchRequestSettings) {
  SimSubresourceRequest patch_b_table(kPatchBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_b_glyph(kPatchBGlyphUrl,
                                      "application/octet-stream");
  DOMWrapperWorld* isolated_world = DOMWrapperWorld::EnsureIsolatedWorld(
      GetDocument().GetExecutionContext()->GetIsolate(), /*world_id=*/1);
  const IftFont* ift_font = LoadIftFont("b", isolated_world);
  ASSERT_NE(ift_font, nullptr);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFetching);

  for (const char* url : {kPatchBTableUrl, kPatchBGlyphUrl}) {
    SCOPED_TRACE(url);
    const Resource* resource =
        GetDocument().Fetcher()->CachedResource(KURL(url));
    ASSERT_NE(resource, nullptr);

    const ResourceRequestHead& request = resource->GetResourceRequest();
    EXPECT_EQ(request.HttpMethod(), http_names::kGET);
    EXPECT_EQ(request.Url(), KURL(url));
    EXPECT_EQ(request.GetRequestContext(),
              mojom::blink::RequestContextType::FONT);
    EXPECT_EQ(request.GetRequestDestination(),
              network::mojom::RequestDestination::kFont);
    EXPECT_EQ(request.ReferrerString(),
              "https://example.com/fonts/roboto-ift.ttf");
    EXPECT_EQ(request.GetMode(), network::mojom::RequestMode::kCors);
    EXPECT_EQ(request.GetCredentialsMode(),
              network::mojom::CredentialsMode::kSameOrigin);
    EXPECT_EQ(resource->Options().initiator_info.name,
              fetch_initiator_type_names::kFont);
    EXPECT_EQ(resource->Options().WorldForCsp(), isolated_world);
  }

  patch_b_table.Complete(PatchPayload(kPatchBTableUrl));
  patch_b_glyph.Complete(PatchPayload(kPatchBGlyphUrl));
}

// Patch URLs are resolved against, and the referrer is set to, the font's
// post-redirect URL.
TEST_F(IftCustomFontDataTest, PatchRequestsUseRedirectedFontUrl) {
  constexpr char kRedirectedFontUrl[] = "https://example.com/v2/roboto-ift.ttf";
  constexpr char kRedirectedPatchBTableUrl[] =
      "https://example.com/v2/1K.ift_tk";
  constexpr char kRedirectedPatchBGlyphUrl[] =
      "https://example.com/v2/04.1.ift_gk";
  SimSubresourceRequest patch_b_table(kRedirectedPatchBTableUrl,
                                      "application/octet-stream");
  SimSubresourceRequest patch_b_glyph(kRedirectedPatchBGlyphUrl,
                                      "application/octet-stream");
  const IftFont* ift_font =
      LoadIftFont("b", /*world=*/nullptr, kRedirectedFontUrl);
  ASSERT_NE(ift_font, nullptr);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFetching);

  for (const char* url :
       {kRedirectedPatchBTableUrl, kRedirectedPatchBGlyphUrl}) {
    SCOPED_TRACE(url);
    const Resource* resource =
        GetDocument().Fetcher()->CachedResource(KURL(url));
    ASSERT_NE(resource, nullptr);
    EXPECT_EQ(resource->GetResourceRequest().ReferrerString(),
              kRedirectedFontUrl);
  }

  patch_b_table.Complete(PatchPayload(kRedirectedPatchBTableUrl));
  patch_b_glyph.Complete(PatchPayload(kRedirectedPatchBGlyphUrl));
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kIdle);
}

TEST_F(IftCustomFontDataTest, StylesShareIftFontButNotCustomFontData) {
  const IftFont* ift_font = LoadIftFont("a");
  ASSERT_NE(ift_font, nullptr);
  Compositor().BeginFrame();
  Persistent<const SimpleFontData> regular_font_data = TargetFontData();
  ASSERT_NE(regular_font_data.Get(), nullptr);

  SetTargetStyle("font-weight: 300");
  Compositor().BeginFrame();
  const SimpleFontData* light_font_data = TargetFontData();
  ASSERT_NE(light_font_data, nullptr);

  const IftCustomFontData* regular = GetIftCustomFontData(regular_font_data);
  const IftCustomFontData* light = GetIftCustomFontData(light_font_data);
  ASSERT_NE(regular, nullptr);
  ASSERT_NE(light, nullptr);
  EXPECT_NE(regular, light);
  EXPECT_EQ(regular->GetIftFont(), ift_font);
  EXPECT_EQ(light->GetIftFont(), ift_font);
  EXPECT_THAT(regular->GetVariationAxesForTesting(),
              testing::Contains(FontVariationAxis(kWghtTag, 400)));
  EXPECT_THAT(light->GetVariationAxesForTesting(),
              testing::Contains(FontVariationAxis(kWghtTag, 300)));
}

TEST_F(IftCustomFontDataTest, DefaultStyleRecordsBuiltInVariationAxes) {
  const IftFont* ift_font = LoadIftFont("a");
  ASSERT_NE(ift_font, nullptr);

  SetTargetStyle("font-size: 20px");
  Compositor().BeginFrame();
  ASSERT_TRUE(IsShapedWith(ift_font));
  const IftCustomFontData* default_axes =
      GetIftCustomFontData(TargetFontData());
  ASSERT_NE(default_axes, nullptr);
  EXPECT_THAT(default_axes->GetVariationAxesForTesting(),
              testing::UnorderedElementsAre(FontVariationAxis(kWghtTag, 400),
                                            FontVariationAxis(kWdthTag, 100),
                                            FontVariationAxis(kItalTag, 0),
                                            FontVariationAxis(kSlntTag, 0),
                                            FontVariationAxis(kOpszTag, 20)));
}

TEST_F(IftCustomFontDataTest,
       StylePropertiesAndVariationSettingsRecordUnclampedAndOverriddenAxes) {
  const IftFont* ift_font = LoadIftFont("a");
  ASSERT_NE(ift_font, nullptr);

  // Stretch beyond the base font's current `fvar` range (`wdth` max 100),
  // oblique angle negation, `font-optical-sizing: none` omitting `opsz`, and
  // custom/duplicate tags in `font-variation-settings` overriding
  // `font-weight: 700`.
  SetTargetStyle(
      "font-weight: 700; font-stretch: 150%; font-style: oblique 10deg; "
      "font-optical-sizing: none; "
      "font-variation-settings: 'GRAD' -50, 'wght' 250, 'wght' 350");
  Compositor().BeginFrame();
  ASSERT_TRUE(IsShapedWith(ift_font));
  const IftCustomFontData* custom_axes = GetIftCustomFontData(TargetFontData());
  ASSERT_NE(custom_axes, nullptr);
  EXPECT_THAT(custom_axes->GetVariationAxesForTesting(),
              testing::UnorderedElementsAre(FontVariationAxis(kWghtTag, 350),
                                            FontVariationAxis(kWdthTag, 150),
                                            FontVariationAxis(kItalTag, 0),
                                            FontVariationAxis(kSlntTag, -10),
                                            FontVariationAxis(kGradTag, -50)));

  // The base font supports wght [100, 400] and has no patches for the other
  // axes, so no patch fetches were triggered.
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kIdle);
}

TEST_F(IftCustomFontDataTest,
       DisabledItalicAxisFeatureOmitsItalFromCustomFontData) {
  const IftFont* ift_font = LoadIftFont("a");
  ASSERT_NE(ift_font, nullptr);

  ScopedFontVariableItalicAxisForTest italic_axis(false);
  SetTargetStyle("font-style: italic");
  Compositor().BeginFrame();
  ASSERT_TRUE(IsShapedWith(ift_font));
  const IftCustomFontData* no_ital_axes =
      GetIftCustomFontData(TargetFontData());
  ASSERT_NE(no_ital_axes, nullptr);
  EXPECT_THAT(no_ital_axes->GetVariationAxesForTesting(),
              testing::Not(testing::Contains(
                  testing::Property(&FontVariationAxis::Tag, kItalTag))));
}

TEST_F(IftCustomFontDataTest, SupportedDesignSpaceDoesNotRequireFetch) {
  const IftFont* ift_font = LoadIftFont();
  ASSERT_NE(ift_font, nullptr);

  const FontVariationAxis supported_axes[] = {{kWghtTag, 300}};
  EXPECT_TRUE(ift_font->RequireSubset("a", supported_axes));

  // Empty text does not fetch design-space patches even for an unpatched axis.
  const FontVariationAxis bold_axes[] = {{kWghtTag, 700}};
  EXPECT_TRUE(ift_font->RequireSubset("", bold_axes));

  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kIdle);
}

TEST_F(IftCustomFontDataTest, FontWeightAppliesDesignSpacePatch) {
  SimSubresourceRequest patch_wght_table(kPatchWghtTableUrl,
                                         "application/octet-stream");
  const IftFont* ift_font = LoadIftFont("a");
  ASSERT_NE(ift_font, nullptr);

  // wght 700 is not in the base font, so the text falls back until the patch
  // is applied.
  SetTargetStyle("font-weight: 700");
  Compositor().BeginFrame();
  EXPECT_FALSE(IsShapedWith(ift_font));
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kCollecting);

  test::RunPendingTasks();
  patch_wght_table.Complete(PatchPayload(kPatchWghtTableUrl));
  test::RunPendingTasks();
  Compositor().BeginFrame();
  EXPECT_TRUE(IsShapedWithUpdated(ift_font));
  const SimpleFontData* font_data = TargetFontData();
  ASSERT_NE(font_data, nullptr);
  // The patched `fvar` covers wght [100, 900], so bold is rendered natively
  // rather than synthesized on top of wght 400.
  EXPECT_FALSE(font_data->PlatformData().SyntheticBold());
  const IftCustomFontData* custom_font_data = GetIftCustomFontData(font_data);
  ASSERT_NE(custom_font_data, nullptr);
  EXPECT_THAT(custom_font_data->GetVariationAxesForTesting(),
              testing::Contains(FontVariationAxis(kWghtTag, 700)));
}

// Variation axes are clamped to explicit `@font-face` descriptors
// (`font-weight`, `font-stretch`, `font-style`), since those are the
// coordinates the font is rendered at.
TEST_F(IftCustomFontDataTest, FontFaceDescriptorsClampDesignSpace) {
  font_face_descriptors_ =
      "font-weight: 100 400; font-stretch: 80% 120%; "
      "font-style: oblique 0deg 8deg;";
  const IftFont* ift_font = LoadIftFont("a");
  ASSERT_NE(ift_font, nullptr);

  SetTargetStyle(
      "font-weight: 700; font-stretch: 50%; font-style: oblique 15deg");
  Compositor().BeginFrame();
  EXPECT_TRUE(IsShapedWith(ift_font));
  const IftCustomFontData* custom_font_data =
      GetIftCustomFontData(TargetFontData());
  ASSERT_NE(custom_font_data, nullptr);
  EXPECT_THAT(custom_font_data->GetVariationAxesForTesting(),
              testing::IsSupersetOf({FontVariationAxis(kWghtTag, 400),
                                     FontVariationAxis(kWdthTag, 80),
                                     FontVariationAxis(kSlntTag, -8)}));
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kIdle);
}

// The `@font-face` `font-variation-settings` descriptor is not part of the
// element's style, but it is part of the design space the font is rendered at.
TEST_F(IftCustomFontDataTest,
       FontVariationSettingsDescriptorAppliesDesignSpacePatch) {
  SimSubresourceRequest patch_wght_table(kPatchWghtTableUrl,
                                         "application/octet-stream");
  font_face_descriptors_ = "font-variation-settings: 'wght' 700;";
  const IftFont* ift_font = LoadIftFont("a");
  ASSERT_NE(ift_font, nullptr);

  Compositor().BeginFrame();
  EXPECT_FALSE(IsShapedWith(ift_font));
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kCollecting);

  test::RunPendingTasks();
  patch_wght_table.Complete(PatchPayload(kPatchWghtTableUrl));
  test::RunPendingTasks();
  Compositor().BeginFrame();
  EXPECT_TRUE(IsShapedWithUpdated(ift_font));
  const IftCustomFontData* custom_font_data =
      GetIftCustomFontData(TargetFontData());
  ASSERT_NE(custom_font_data, nullptr);
  EXPECT_THAT(custom_font_data->GetVariationAxesForTesting(),
              testing::Contains(FontVariationAxis(kWghtTag, 700)));
}

// The `@font-face` `size-adjust` descriptor is not part of the element's style,
// but it changes the optical size the font is rendered at.
TEST_F(IftCustomFontDataTest, SizeAdjustDescriptorAppliesToOpticalSize) {
  font_face_descriptors_ = "size-adjust: 50%;";
  const IftFont* ift_font = LoadIftFont("a");
  ASSERT_NE(ift_font, nullptr);

  SetTargetStyle("font-size: 32px");
  Compositor().BeginFrame();
  ASSERT_TRUE(IsShapedWith(ift_font));
  const IftCustomFontData* scaled_axes = GetIftCustomFontData(TargetFontData());
  ASSERT_NE(scaled_axes, nullptr);
  EXPECT_THAT(scaled_axes->GetVariationAxesForTesting(),
              testing::Contains(FontVariationAxis(kOpszTag, 16)));

  // `font-size: 0px` with `size-adjust` resolves `opsz` to 0 rather than NaN.
  SetTargetStyle("font-size: 0px");
  Compositor().BeginFrame();
  ASSERT_TRUE(IsShapedWith(ift_font));
  const IftCustomFontData* zero_axes = GetIftCustomFontData(TargetFontData());
  ASSERT_NE(zero_axes, nullptr);
  EXPECT_THAT(zero_axes->GetVariationAxesForTesting(),
              testing::Contains(FontVariationAxis(kOpszTag, 0)));
}

TEST_F(IftCustomFontDataTest,
       UnsupportedDesignSpaceDoesNotBlockSupportedSibling) {
  SimSubresourceRequest patch_wght_table(kPatchWghtTableUrl,
                                         "application/octet-stream");
  const IftFont* ift_font = LoadIftFont("");
  ASSERT_NE(ift_font, nullptr);

  Element* target = GetElementById("target");
  ASSERT_NE(target, nullptr);
  target->setTextContent(String());

  Element* regular = GetDocument().CreateRawElement(html_names::kSpanTag);
  regular->setAttribute(html_names::kIdAttr, AtomicString("regular"));
  regular->setTextContent(String("a"));
  target->appendChild(regular);

  Element* bold = GetDocument().CreateRawElement(html_names::kSpanTag);
  bold->setAttribute(html_names::kIdAttr, AtomicString("bold"));
  bold->setAttribute(html_names::kStyleAttr, AtomicString("font-weight: 700"));
  bold->setTextContent(String("a"));
  target->appendChild(bold);

  // `wght 400` shapes immediately with `ift_font`, while `wght 700` falls back
  // and triggers the design-space patch fetch.
  Compositor().BeginFrame();
  EXPECT_EQ(GetIftFont(ElementFontData("regular")), ift_font);
  EXPECT_NE(GetIftFont(ElementFontData("bold")), ift_font);
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kCollecting);

  test::RunPendingTasks();
  patch_wght_table.Complete(PatchPayload(kPatchWghtTableUrl));
  test::RunPendingTasks();
  Compositor().BeginFrame();

  const SimpleFontData* regular_font = ElementFontData("regular");
  const SimpleFontData* bold_font = ElementFontData("bold");
  ASSERT_NE(regular_font, nullptr);
  ASSERT_NE(bold_font, nullptr);
  EXPECT_EQ(GetIftFont(regular_font), ift_font);
  EXPECT_EQ(GetIftFont(bold_font), ift_font);
  EXPECT_NE(regular_font, initial_font_data_.Get());
  EXPECT_NE(bold_font, initial_font_data_.Get());
  EXPECT_FALSE(bold_font->PlatformData().SyntheticBold());
}

TEST_F(IftCustomFontDataTest, CodepointAndDesignSpacePatchesAppliedTogether) {
  SimSubresourceRequest patch_wght_table(kPatchWghtTableUrl,
                                         "application/octet-stream");
  SimSubresourceRequest patch_b_after_wght_table(kPatchBAfterWghtTableUrl,
                                                 "application/octet-stream");
  SimSubresourceRequest patch_b_expanded_glyph(kPatchBExpandedWghtGlyphUrl,
                                               "application/octet-stream");
  const IftFont* ift_font = LoadIftFont("b");
  ASSERT_NE(ift_font, nullptr);

  SetTargetStyle("font-weight: 700");
  Compositor().BeginFrame();
  EXPECT_FALSE(IsShapedWith(ift_font));
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kCollecting);

  // First round applies the wght [500, 900] table patch, which reveals the 'b'
  // table and expanded-weight glyph patches without fetching the initial-weight
  // 'b' outline.
  test::RunPendingTasks();
  ASSERT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFetching);
  patch_wght_table.Complete(PatchPayload(kPatchWghtTableUrl));
  test::RunPendingTasks();
  ASSERT_EQ(ift_font->GetStateForTesting(), IftFont::State::kFetching);

  patch_b_after_wght_table.Complete(PatchPayload(kPatchBAfterWghtTableUrl));
  patch_b_expanded_glyph.Complete(PatchPayload(kPatchBExpandedWghtGlyphUrl));
  test::RunPendingTasks();
  EXPECT_EQ(ift_font->GetStateForTesting(), IftFont::State::kIdle);

  Compositor().BeginFrame();
  EXPECT_TRUE(IsShapedWithUpdated(ift_font));
  const SimpleFontData* font_data = TargetFontData();
  ASSERT_NE(font_data, nullptr);
  EXPECT_FALSE(font_data->PlatformData().SyntheticBold());
  const FontVariationAxis bold_axes[] = {{kWghtTag, 700}};
  EXPECT_TRUE(ift_font->RequireSubset("b", bold_axes));
}

}  // namespace
}  // namespace blink
