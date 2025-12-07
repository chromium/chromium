// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/font_variant_emoji.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {
namespace test {

namespace {

class TestFontSelector : public FontSelector {
 public:
  static TestFontSelector* Create(const String& path) {
    std::optional<Vector<char>> data = test::ReadFromFile(path);
    CHECK(data);
    scoped_refptr<SharedBuffer> font_buffer =
        SharedBuffer::Create(std::move(*data));
    String ots_parse_message;
    return MakeGarbageCollected<TestFontSelector>(
        FontCustomPlatformData::Create(font_buffer.get(), ots_parse_message));
  }

  static TestFontSelector* Create(base::span<const uint8_t> data) {
    scoped_refptr<SharedBuffer> font_buffer = SharedBuffer::Create(data);
    String ots_parse_message;
    FontCustomPlatformData* font_custom_platform_data =
        FontCustomPlatformData::Create(font_buffer.get(), ots_parse_message);
    if (!font_custom_platform_data)
      return nullptr;
    return MakeGarbageCollected<TestFontSelector>(
        std::move(font_custom_platform_data));
  }

  TestFontSelector(FontCustomPlatformData* custom_platform_data)
      : custom_platform_data_(custom_platform_data) {
    DCHECK(custom_platform_data_);
  }
  ~TestFontSelector() override = default;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(custom_platform_data_);
    FontSelector::Trace(visitor);
  }

  FontData* GetFontData(const FontDescription& font_description,
                        const FontFamily&) override {
    FontSelectionCapabilities normal_capabilities(
        {kNormalWidthValue, kNormalWidthValue},
        {kNormalSlopeValue, kNormalSlopeValue},
        {kNormalWeightValue, kNormalWeightValue});
    const FontPlatformData* platform_data =
        custom_platform_data_->GetFontPlatformData(
            font_description.EffectiveFontSize(),
            font_description.AdjustedSpecifiedSize(),
            font_description.IsSyntheticBold() &&
                font_description.SyntheticBoldAllowed(),
            font_description.IsSyntheticItalic() &&
                font_description.SyntheticItalicAllowed(),
            font_description.GetFontSelectionRequest(), normal_capabilities,
            font_description.FontOpticalSizing(),
            font_description.TextRendering(), {},
            font_description.Orientation());
    return MakeGarbageCollected<SimpleFontData>(
        platform_data, MakeGarbageCollected<CustomFontData>());
  }

  void WillUseFontData(const FontDescription&,
                       const FontFamily& family,
                       const String& text) override {}
  void WillUseRange(const FontDescription&,
                    const AtomicString& family_name,
                    const FontDataForRangeSet&) override {}

  unsigned Version() const override { return 0; }
  void FontCacheInvalidated() override {}
  void ReportNotDefGlyph() const override {}
  ExecutionContext* GetExecutionContext() const override { return nullptr; }
  FontFaceCache* GetFontFaceCache() override { return nullptr; }

  void RegisterForInvalidationCallbacks(FontSelectorClient*) override {}
  void UnregisterForInvalidationCallbacks(FontSelectorClient*) override {}

  bool IsPlatformFamilyMatchAvailable(
      const FontDescription&,
      const FontFamily& passed_family) override {
    return false;
  }

 private:
  Member<FontCustomPlatformData> custom_platform_data_;
};

}  // namespace

Font* CreateTestFont(const AtomicString& family_name,
                     base::span<const uint8_t> data,
                     float size,
                     const FontDescription::VariantLigatures* ligatures) {
  FontDescription font_description;
  font_description.SetFamily(
      FontFamily(family_name, FontFamily::Type::kFamilyName));
  font_description.SetSpecifiedSize(size);
  font_description.SetComputedSize(size);
  if (ligatures)
    font_description.SetVariantLigatures(*ligatures);

  return MakeGarbageCollected<Font>(font_description,
                                    TestFontSelector::Create(data));
}

Font* CreateTestFont(const AtomicString& family_name,
                     const String& font_path,
                     float size,
                     const FontDescription::VariantLigatures* ligatures,
                     const FontVariantEmoji variant_emoji,
                     void (*init_font_description)(FontDescription*)) {
  FontDescription font_description;
  font_description.SetFamily(
      FontFamily(family_name, FontFamily::Type::kFamilyName));
  font_description.SetSpecifiedSize(size);
  font_description.SetComputedSize(size);
  font_description.SetVariantEmoji(variant_emoji);
  if (ligatures)
    font_description.SetVariantLigatures(*ligatures);
  if (init_font_description)
    (*init_font_description)(&font_description);

  return MakeGarbageCollected<Font>(font_description,
                                    TestFontSelector::Create(font_path));
}

Font* CreateAhemFont(float size) {
  return CreateTestFont(AtomicString("Ahem"), PlatformTestDataPath("Ahem.woff"),
                        size);
}

#if BUILDFLAG(IS_WIN)
void TestFontPrewarmer::PrewarmFamily(const WebString& family_name) {
  family_names_.push_back(family_name);
}

ScopedTestFontPrewarmer::ScopedTestFontPrewarmer()
    : saved_(FontCache::GetFontPrewarmer()) {
  FontCache::SetFontPrewarmer(&current_);
}

ScopedTestFontPrewarmer::~ScopedTestFontPrewarmer() {
  FontCache::SetFontPrewarmer(saved_);
}
#endif

}  // namespace test
}  // namespace blink
