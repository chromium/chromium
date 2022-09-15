// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_thread_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <string>

namespace content {

class RenderThreadImplUnittest : public testing::Test {
 public:
  RenderThreadImplUnittest()
      : kCustomizableHistogram_("Histogram1"),
        kNormalHistogram_("Histogram2") {}
  ~RenderThreadImplUnittest() override {}

 protected:
  void SetUp() override {
    histogram_customizer_.custom_histograms_.clear();
    histogram_customizer_.custom_histograms_.insert(kCustomizableHistogram_);
  }
  RenderThreadImpl::HistogramCustomizer histogram_customizer_;
  const char* kCustomizableHistogram_;
  const char* kNormalHistogram_;
};

TEST_F(RenderThreadImplUnittest, CustomHistogramsWithNoNavigations) {
  // First there is no page -> no custom histograms.
  EXPECT_EQ(kCustomizableHistogram_,
            histogram_customizer_.ConvertToCustomHistogramName(
                kCustomizableHistogram_));
  EXPECT_EQ(kNormalHistogram_,
            histogram_customizer_.ConvertToCustomHistogramName(
                kNormalHistogram_));
}

TEST_F(RenderThreadImplUnittest, CustomHistogramsForOneRenderView) {
  histogram_customizer_.RenderViewNavigatedToHost("mail.google.com", 1);
  EXPECT_EQ(std::string(kCustomizableHistogram_) + ".gmail",
            histogram_customizer_.ConvertToCustomHistogramName(
                kCustomizableHistogram_));
  EXPECT_EQ(kNormalHistogram_,
            histogram_customizer_.ConvertToCustomHistogramName(
                kNormalHistogram_));
  histogram_customizer_.RenderViewNavigatedToHost("docs.google.com", 1);
  EXPECT_EQ(std::string(kCustomizableHistogram_) + ".docs",
            histogram_customizer_.ConvertToCustomHistogramName(
                kCustomizableHistogram_));
  histogram_customizer_.RenderViewNavigatedToHost("nottracked.com", 1);
  EXPECT_EQ(kCustomizableHistogram_,
            histogram_customizer_.ConvertToCustomHistogramName(
                kCustomizableHistogram_));
}

TEST_F(RenderThreadImplUnittest, CustomHistogramsForTwoRenderViews) {
  // First there is only one view.
  histogram_customizer_.RenderViewNavigatedToHost("mail.google.com", 1);
  // Second view created and it navigates to the same host -> we can have a
  // custom diagram.
  histogram_customizer_.RenderViewNavigatedToHost("mail.google.com", 2);
  EXPECT_EQ(std::string(kCustomizableHistogram_) + ".gmail",
            histogram_customizer_.ConvertToCustomHistogramName(
                kCustomizableHistogram_));
  EXPECT_EQ(kNormalHistogram_,
            histogram_customizer_.ConvertToCustomHistogramName(
                kNormalHistogram_));
  // Now the views diverge (one of them navigates to a different host) -> no
  // custom diagram.
  histogram_customizer_.RenderViewNavigatedToHost("docs.google.com", 2);
  EXPECT_EQ(kCustomizableHistogram_,
            histogram_customizer_.ConvertToCustomHistogramName(
                kCustomizableHistogram_));
  // After this point, there will never be a custom diagram again, even if the
  // view navigated back to the common host.
  histogram_customizer_.RenderViewNavigatedToHost("mail.google.com", 2);
  EXPECT_EQ(kCustomizableHistogram_,
            histogram_customizer_.ConvertToCustomHistogramName(
                kCustomizableHistogram_));
}

TEST_F(RenderThreadImplUnittest, IdentifyAlexaTop10NonGoogleSite) {
  EXPECT_TRUE(histogram_customizer_.IsAlexaTop10NonGoogleSite("www.amazon.de"));
  EXPECT_TRUE(histogram_customizer_.IsAlexaTop10NonGoogleSite("amazon.de"));
  EXPECT_TRUE(histogram_customizer_.IsAlexaTop10NonGoogleSite("amazon.co.uk"));
  EXPECT_TRUE(
      histogram_customizer_.IsAlexaTop10NonGoogleSite("jp.wikipedia.org"));
  EXPECT_TRUE(
      histogram_customizer_.IsAlexaTop10NonGoogleSite("www.facebook.com"));
  EXPECT_FALSE(histogram_customizer_.IsAlexaTop10NonGoogleSite(""));
  EXPECT_FALSE(
      histogram_customizer_.IsAlexaTop10NonGoogleSite("www.google.com"));
  EXPECT_FALSE(histogram_customizer_.IsAlexaTop10NonGoogleSite("madeup"));
}

}  // namespace content
