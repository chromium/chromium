// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/annotations/ax_image_stopwords.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(AXImageStopwordsTest, EmptyStringIsAStopword) {
  AXImageStopwords& stopwords = AXImageStopwords::GetInstance();
  EXPECT_TRUE(stopwords.IsImageStopword(""));
}

TEST(AXImageStopwordsTest, English) {
  AXImageStopwords& stopwords = AXImageStopwords::GetInstance();
  EXPECT_TRUE(stopwords.IsImageStopword("the"));
  EXPECT_TRUE(stopwords.IsImageStopword("http"));
  EXPECT_TRUE(stopwords.IsImageStopword("for"));
  EXPECT_TRUE(stopwords.IsImageStopword("with"));
  EXPECT_TRUE(stopwords.IsImageStopword("background"));
  EXPECT_TRUE(stopwords.IsImageStopword("sunday"));
  EXPECT_TRUE(stopwords.IsImageStopword("november"));
  EXPECT_FALSE(stopwords.IsImageStopword("cat"));
  EXPECT_FALSE(stopwords.IsImageStopword("obama"));
  EXPECT_FALSE(stopwords.IsImageStopword("heart"));
  EXPECT_FALSE(stopwords.IsImageStopword("home"));
}

TEST(AXImageStopwordsTest, EnglishCaseInsensitive) {
  AXImageStopwords& stopwords = AXImageStopwords::GetInstance();
  EXPECT_TRUE(stopwords.IsImageStopword("the"));
  EXPECT_TRUE(stopwords.IsImageStopword("THE"));
  EXPECT_TRUE(stopwords.IsImageStopword("tHe"));
}

TEST(AXImageStopwordsTest, EnglishAllShortWordsAreStopwords) {
  AXImageStopwords& stopwords = AXImageStopwords::GetInstance();
  // One and two-letter words are always stopwords no matter what
  // characters, as long as they're Latin characters.
  EXPECT_TRUE(stopwords.IsImageStopword("q"));
  EXPECT_TRUE(stopwords.IsImageStopword("I"));
  EXPECT_TRUE(stopwords.IsImageStopword("ff"));
  EXPECT_TRUE(stopwords.IsImageStopword("zU"));

  // Three-letter words aren't stopwords unless they're in our set.
  EXPECT_FALSE(stopwords.IsImageStopword("fff"));
  EXPECT_FALSE(stopwords.IsImageStopword("GGG"));
}

TEST(AXImageStopwordsTest, French) {
  AXImageStopwords& stopwords = AXImageStopwords::GetInstance();
  EXPECT_TRUE(stopwords.IsImageStopword("les"));
  EXPECT_TRUE(stopwords.IsImageStopword("BLANC"));
  EXPECT_TRUE(stopwords.IsImageStopword("FÉVR"));
  EXPECT_FALSE(stopwords.IsImageStopword("modèle"));
  EXPECT_FALSE(stopwords.IsImageStopword("recettes"));
  EXPECT_FALSE(stopwords.IsImageStopword("ciel"));
}

TEST(AXImageStopwordsTest, Italian) {
  AXImageStopwords& stopwords = AXImageStopwords::GetInstance();
  EXPECT_TRUE(stopwords.IsImageStopword("con"));
  EXPECT_TRUE(stopwords.IsImageStopword("PIÙ"));
  EXPECT_TRUE(stopwords.IsImageStopword("Immagini"));
  EXPECT_FALSE(stopwords.IsImageStopword("montagna"));
  EXPECT_FALSE(stopwords.IsImageStopword("pubblico"));
  EXPECT_FALSE(stopwords.IsImageStopword("Cultura"));
}

TEST(AXImageStopwordsTest, German) {
  AXImageStopwords& stopwords = AXImageStopwords::GetInstance();
  EXPECT_TRUE(stopwords.IsImageStopword("und"));
  EXPECT_TRUE(stopwords.IsImageStopword("FÜR"));
  EXPECT_TRUE(stopwords.IsImageStopword("sprüche"));
  EXPECT_FALSE(stopwords.IsImageStopword("haus"));
  EXPECT_FALSE(stopwords.IsImageStopword("gesichter"));
  EXPECT_FALSE(stopwords.IsImageStopword("Deutsche"));
}

TEST(AXImageStopwordsTest, Spanish) {
  AXImageStopwords& stopwords = AXImageStopwords::GetInstance();
  EXPECT_TRUE(stopwords.IsImageStopword("con"));
  EXPECT_TRUE(stopwords.IsImageStopword("MÁS"));
  EXPECT_TRUE(stopwords.IsImageStopword("enero"));
  EXPECT_FALSE(stopwords.IsImageStopword("tortuga"));
  EXPECT_FALSE(stopwords.IsImageStopword("flores"));
  EXPECT_FALSE(stopwords.IsImageStopword("actividades"));
}

TEST(AXImageStopwordsTest, Hindi) {
  AXImageStopwords& stopwords = AXImageStopwords::GetInstance();
  EXPECT_TRUE(stopwords.IsImageStopword("रहे"));
  EXPECT_TRUE(stopwords.IsImageStopword("चित्र"));
  EXPECT_TRUE(stopwords.IsImageStopword("वीडियो"));
  EXPECT_FALSE(stopwords.IsImageStopword("जानिए"));
  EXPECT_FALSE(stopwords.IsImageStopword("भारतीय"));
}

TEST(AXImageStopwordsTest, HindiShortWordsAreStopwords) {
  AXImageStopwords& stopwords = AXImageStopwords::GetInstance();
  // All Hindi words with two or fewer unicode codepoints are stopwords.
  // Note that these appear to have just one glyph, but they're encoded
  // as two codepoints, a consonant and a vowel.
  EXPECT_TRUE(stopwords.IsImageStopword("की"));  // "of"
  EXPECT_TRUE(stopwords.IsImageStopword("में"));   // "in"

  // Some two-character Hindi words are stopwords, they're in our set.
  EXPECT_TRUE(stopwords.IsImageStopword("एक"));  // "one"

  // Other two-character Hindi words are not stopwords.
  EXPECT_FALSE(stopwords.IsImageStopword("सदा"));   // "always"
  EXPECT_FALSE(stopwords.IsImageStopword("खाना"));  // "food"
}

}  // namespace content
