// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"

#include "base/memory/scoped_refptr.h"
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

  TestFontSelector(scoped_refptr<FontCustomPlatformData> custom_platform_data)
      : custom_platform_data_(std::move(custom_platform_data)) {
    DCHECK(custom_platform_data_);
  }
  ~TestFontSelector() override = default;

  scoped_refptr<FontData> GetFontData(
      const FontDescription& font_description,
      const AtomicString& family_name) override {
    FontSelectionCapabilities normal_capabilities(
        {NormalWidthValue(), NormalWidthValue()},
        {NormalSlopeValue(), NormalSlopeValue()},
        {NormalWeightValue(), NormalWeightValue()});
    FontPlatformData platform_data = custom_platform_data_->GetFontPlatformData(
        font_description.EffectiveFontSize(),
        font_description.IsSyntheticBold(),
        font_description.IsSyntheticItalic(),
        font_description.GetFontSelectionRequest(), normal_capabilities,
        font_description.FontOpticalSizing(), font_description.Orientation());
    return SimpleFontData::Create(platform_data, CustomFontData::Create());
  }

  void WillUseFontData(const FontDescription&,
                       const AtomicString& family_name,
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
      const AtomicString& passed_family) override {
    return false;
  }

 private:
  scoped_refptr<FontCustomPlatformData> custom_platform_data_;
};

}  // namespace

Font CreateTestFont(const AtomicString& family_name,
                    const String& font_path,
                    float size,
                    const FontDescription::VariantLigatures* ligatures) {
  FontFamily family;
  family.SetFamily(family_name);

  FontDescription font_description;
  font_description.SetFamily(family);
  font_description.SetSpecifiedSize(size);
  font_description.SetComputedSize(size);
  if (ligatures)
    font_description.SetVariantLigatures(*ligatures);

  Font font(font_description);
  font.Update(TestFontSelector::Create(font_path));
  return font;
}

}  // namespace test
}  // namespace blink
