// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {
namespace test {

namespace {

class TestFontSelector : public FontSelector {
 public:
  static TestFontSelector* Create(const String& path) {
    scoped_refptr<SharedBuffer> font_buffer = test::ReadFromFile(path);
    String ots_parse_message;
    return MakeGarbageCollected<TestFontSelector>(
        FontCustomPlatformData::Create(font_buffer.get(), ots_parse_message));
  }

  static TestFontSelector* Create(const uint8_t* data, size_t size) {
    scoped_refptr<SharedBuffer> font_buffer = SharedBuffer::Create(data, size);
    String ots_parse_message;
    scoped_refptr<FontCustomPlatformData> font_custom_platform_data =
        FontCustomPlatformData::Create(font_buffer.get(), ots_parse_message);
    if (!font_custom_platform_data)
      return nullptr;
    return MakeGarbageCollected<TestFontSelector>(
        std::move(font_custom_platform_data));
  }

  TestFontSelector(scoped_refptr<FontCustomPlatformData> custom_platform_data)
      : custom_platform_data_(std::move(custom_platform_data)) {
    DCHECK(custom_platform_data_);
  }
  ~TestFontSelector() override = default;

  scoped_refptr<FontData> GetFontData(const FontDescription& font_description,
                                      const FontFamily&) override {
    FontSelectionCapabilities normal_capabilities(
        {NormalWidthValue(), NormalWidthValue()},
        {NormalSlopeValue(), NormalSlopeValue()},
        {NormalWeightValue(), NormalWeightValue()});
    FontPlatformData platform_data = custom_platform_data_->GetFontPlatformData(
        font_description.EffectiveFontSize(),
        font_description.AdjustedSpecifiedSize(),
        font_description.IsSyntheticBold() &&
            font_description.SyntheticBoldAllowed(),
        font_description.IsSyntheticItalic() &&
            font_description.SyntheticItalicAllowed(),
        font_description.GetFontSelectionRequest(), normal_capabilities,
        font_description.FontOpticalSizing(), font_description.TextRendering(),
        {}, font_description.Orientation());
    return SimpleFontData::Create(platform_data, CustomFontData::Create());
  }

  void WillUseFontData(const FontDescription&,
                       const FontFamily& family,
                       const String& text) override {}
  void WillUseRange(const FontDescription&,
                    const AtomicString& family_name,
                    const FontDataForRangeSet&) override {}

  unsigned Version() const override { return 0; }
  void FontCacheInvalidated() override {}
  void ReportSuccessfulFontFamilyMatch(
      const AtomicString& font_family_name) override {}
  void ReportFailedFontFamilyMatch(
      const AtomicString& font_family_name) override {}
  void ReportSuccessfulLocalFontMatch(const AtomicString& font_name) override {}
  void ReportFailedLocalFontMatch(const AtomicString& font_name) override {}
  void ReportFontLookupByUniqueOrFamilyName(
      const AtomicString& name,
      const FontDescription& font_description,
      scoped_refptr<SimpleFontData> resulting_font_data) override {}
  void ReportFontLookupByUniqueNameOnly(
      const AtomicString& name,
      const FontDescription& font_description,
      scoped_refptr<SimpleFontData> resulting_font_data,
      bool is_loading_fallback = false) override {}
  void ReportFontLookupByFallbackCharacter(
      UChar32 hint,
      FontFallbackPriority fallback_priority,
      const FontDescription& font_description,
      scoped_refptr<SimpleFontData> resulting_font_data) override {}
  void ReportLastResortFallbackFontLookup(
      const FontDescription& font_description,
      scoped_refptr<SimpleFontData> resulting_font_data) override {}
  void ReportNotDefGlyph() const override {}
  void ReportEmojiSegmentGlyphCoverage(unsigned, unsigned) override {}
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
  scoped_refptr<FontCustomPlatformData> custom_platform_data_;
};

}  // namespace

Font CreateTestFont(const AtomicString& family_name,
                    const uint8_t* data,
                    size_t data_size,
                    float size,
                    const FontDescription::VariantLigatures* ligatures) {
  FontFamily family;
  family.SetFamily(family_name, FontFamily::Type::kFamilyName);

  FontDescription font_description;
  font_description.SetFamily(family);
  font_description.SetSpecifiedSize(size);
  font_description.SetComputedSize(size);
  if (ligatures)
    font_description.SetVariantLigatures(*ligatures);

  return Font(font_description, TestFontSelector::Create(data, data_size));
}

Font CreateTestFont(const AtomicString& family_name,
                    const String& font_path,
                    float size,
                    const FontDescription::VariantLigatures* ligatures,
                    void (*init_font_description)(FontDescription*)) {
  FontFamily family;
  family.SetFamily(family_name, FontFamily::Type::kFamilyName);

  FontDescription font_description;
  font_description.SetFamily(family);
  font_description.SetSpecifiedSize(size);
  font_description.SetComputedSize(size);
  if (ligatures)
    font_description.SetVariantLigatures(*ligatures);
  if (init_font_description)
    (*init_font_description)(&font_description);

  return Font(font_description, TestFontSelector::Create(font_path));
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
