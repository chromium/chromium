// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/hyphenation.h"

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"

using testing::ElementsAre;
using testing::ElementsAreArray;

#if defined(OS_ANDROID)
#define USE_MINIKIN_HYPHENATION
#endif
#if defined(USE_MINIKIN_HYPHENATION)
#include "base/files/file_path.h"
#include "third_party/blink/renderer/platform/text/hyphenation/hyphenation_minikin.h"
#endif

namespace blink {

class NoHyphenation : public Hyphenation {
 public:
  wtf_size_t LastHyphenLocation(const StringView&,
                                wtf_size_t before_index) const override {
    return 0;
  }
};

class HyphenationTest : public testing::Test {
 protected:
  void TearDown() override { LayoutLocale::ClearForTesting(); }

#if defined(USE_MINIKIN_HYPHENATION) || defined(OS_MACOSX)
  // Get a |Hyphenation| instnace for the specified locale for testing.
  scoped_refptr<Hyphenation> GetHyphenation(const AtomicString& locale) {
#if defined(USE_MINIKIN_HYPHENATION)
    // Because the mojo service to open hyphenation dictionaries is not
    // accessible from the unit test, open the dictionary file directly for
    // testing.
    std::string filename = "hyph-" + locale.Ascii() + ".hyb";
#if defined(OS_ANDROID)
    base::FilePath path("/system/usr/hyphen-data");
#else
#error "This configuration is not supported."
#endif
    path = path.AppendASCII(filename);
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (file.IsValid())
      return HyphenationMinikin::FromFileForTesting(std::move(file));
#else
    if (const LayoutLocale* layout_locale = LayoutLocale::Get(locale))
      return layout_locale->GetHyphenation();
#endif
    return nullptr;
  }
#endif
};

TEST_F(HyphenationTest, Get) {
  scoped_refptr<Hyphenation> hyphenation = base::AdoptRef(new NoHyphenation);
  LayoutLocale::SetHyphenationForTesting("en-US", hyphenation);
  EXPECT_EQ(hyphenation.get(), LayoutLocale::Get("en-US")->GetHyphenation());

  LayoutLocale::SetHyphenationForTesting("en-UK", nullptr);
  EXPECT_EQ(nullptr, LayoutLocale::Get("en-UK")->GetHyphenation());
}

#if defined(USE_MINIKIN_HYPHENATION) || defined(OS_MACOSX)
// TODO(crbug.com/851413): Reenable this test.
#if defined(OS_ANDROID)
#define MAYBE_HyphenLocations DISABLED_HyphenLocations
#else
#define MAYBE_HyphenLocations HyphenLocations
#endif
TEST_F(HyphenationTest, MAYBE_HyphenLocations) {
  scoped_refptr<Hyphenation> hyphenation = GetHyphenation("en-us");
  ASSERT_TRUE(hyphenation) << "Cannot find the hyphenation engine";

  // Get all hyphenation points by |HyphenLocations|.
  const String word("hyphenation");
  Vector<wtf_size_t, 8> locations = hyphenation->HyphenLocations(word);
  EXPECT_GT(locations.size(), 0u);

  for (wtf_size_t i = 1; i < locations.size(); i++) {
    ASSERT_GT(locations[i - 1], locations[i])
        << "hyphenLocations must return locations in the descending order";
  }

  // Test |LastHyphenLocation| returns all hyphenation points.
  Vector<wtf_size_t, 8> actual;
  for (wtf_size_t offset = word.length();;) {
    offset = hyphenation->LastHyphenLocation(word, offset);
    if (!offset)
      break;
    actual.push_back(offset);
  }
  EXPECT_THAT(actual, ElementsAreArray(locations));

  // Test |FirstHyphenLocation| returns all hyphenation points.
  actual.clear();
  for (wtf_size_t offset = 0;;) {
    offset = hyphenation->FirstHyphenLocation(word, offset);
    if (!offset)
      break;
    actual.push_back(offset);
  }
  locations.Reverse();
  EXPECT_THAT(actual, ElementsAreArray(locations));
}

TEST_F(HyphenationTest, LeadingSpaces) {
  scoped_refptr<Hyphenation> hyphenation = GetHyphenation("en-us");
#if defined(OS_ANDROID)
  // Hyphenation is available only for Android M MR1 or later.
  if (!hyphenation)
    return;
#endif
  ASSERT_TRUE(hyphenation) << "Cannot find the hyphenation for en-us";

  String leading_space(" principle");
  EXPECT_THAT(hyphenation->HyphenLocations(leading_space), ElementsAre(7, 5));
  EXPECT_EQ(5u, hyphenation->LastHyphenLocation(leading_space, 6));

  String multi_leading_spaces("   principle");
  EXPECT_THAT(hyphenation->HyphenLocations(multi_leading_spaces),
              ElementsAre(9, 7));
  EXPECT_EQ(7u, hyphenation->LastHyphenLocation(multi_leading_spaces, 8));

  // Line breaker is not supposed to pass only spaces, no locations.
  String only_spaces("   ");
  EXPECT_THAT(hyphenation->HyphenLocations(only_spaces), ElementsAre());
  EXPECT_EQ(0u, hyphenation->LastHyphenLocation(only_spaces, 3));

  // Line breaker is not supposed to pass with trailing spaces.
  String trailing_space("principle ");
  EXPECT_THAT(hyphenation->HyphenLocations(trailing_space),
              testing::AnyOf(ElementsAre(), ElementsAre(6, 4)));
  EXPECT_EQ(0u, hyphenation->LastHyphenLocation(trailing_space, 10));
}

TEST_F(HyphenationTest, English) {
  scoped_refptr<Hyphenation> hyphenation = GetHyphenation("en-us");
#if defined(OS_ANDROID)
  // Hyphenation is available only for Android M MR1 or later.
  if (!hyphenation)
    return;
#endif
  ASSERT_TRUE(hyphenation) << "Cannot find the hyphenation for en-us";

  Vector<wtf_size_t, 8> locations = hyphenation->HyphenLocations("hyphenation");
  EXPECT_THAT(locations, testing::AnyOf(ElementsAreArray({6, 2}),
                                        ElementsAreArray({7, 6, 2})));
}

TEST_F(HyphenationTest, German) {
  scoped_refptr<Hyphenation> hyphenation = GetHyphenation("de-1996");
#if defined(OS_ANDROID)
  // Hyphenation is available only for Android M MR1 or later.
  if (!hyphenation)
    return;
#endif
  ASSERT_TRUE(hyphenation) << "Cannot find the hyphenation for de-1996";

  Vector<wtf_size_t, 8> locations =
      hyphenation->HyphenLocations("konsonantien");
  EXPECT_THAT(locations, ElementsAreArray({8, 5, 3}));

  // Test words with non-ASCII (> U+0080) characters.
  locations = hyphenation->HyphenLocations(
      "B"
      "\xE4"  // LATIN SMALL LETTER A WITH DIAERESIS
      "chlein");
  EXPECT_THAT(locations, ElementsAreArray({4}));
}
#endif

}  // namespace blink
