// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/previews_resource_loading_hints.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class PreviewsResourceLoadingHintsTest : public PageTestBase {
 public:
  PreviewsResourceLoadingHintsTest() {
    dummy_page_holder_ = std::make_unique<DummyPageHolder>(IntSize(1, 1));
  }

 protected:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(PreviewsResourceLoadingHintsTest, NoPatterns) {
  Vector<WTF::String> subresources_to_block;

  PreviewsResourceLoadingHints* hints = PreviewsResourceLoadingHints::Create(
      dummy_page_holder_->GetDocument(), ukm::UkmRecorder::GetNewSourceID(),
      subresources_to_block);
  EXPECT_TRUE(hints->AllowLoad(ResourceType::kScript,
                               KURL("https://www.example.com/"),
                               ResourceLoadPriority::kHighest));
}

TEST_F(PreviewsResourceLoadingHintsTest, OnePattern) {
  Vector<WTF::String> subresources_to_block;
  subresources_to_block.push_back("foo.jpg");

  PreviewsResourceLoadingHints* hints = PreviewsResourceLoadingHints::Create(
      dummy_page_holder_->GetDocument(), ukm::UkmRecorder::GetNewSourceID(),
      subresources_to_block);

  const struct {
    KURL url;
    bool allow_load_expected;
  } tests[] = {
      {KURL("https://www.example.com/"), true},
      {KURL("https://www.example.com/foo.js"), true},
      {KURL("https://www.example.com/foo.jpg"), false},
      {KURL("https://www.example.com/pages/foo.jpg"), false},
      {KURL("https://www.example.com/foobar.jpg"), true},
      {KURL("https://www.example.com/barfoo.jpg"), false},
      {KURL("http://www.example.com/foo.jpg"), false},
      {KURL("http://www.example.com/foo.jpg?q=alpha"), false},
      {KURL("http://www.example.com/bar.jpg?q=foo.jpg"), true},
      {KURL("http://www.example.com/bar.jpg?q=foo.jpg#foo.jpg"), true},
  };

  for (const auto& test : tests) {
    base::HistogramTester histogram_tester;
    // By default, resource blocking hints do not apply to images.
    EXPECT_TRUE(hints->AllowLoad(ResourceType::kImage, test.url,
                                 ResourceLoadPriority::kHighest));
    // By default, resource blocking hints apply to CSS and Scripts.
    EXPECT_EQ(test.allow_load_expected,
              hints->AllowLoad(ResourceType::kCSSStyleSheet, test.url,
                               ResourceLoadPriority::kHighest));
    EXPECT_EQ(test.allow_load_expected,
              hints->AllowLoad(ResourceType::kScript, test.url,
                               ResourceLoadPriority::kHighest));
    histogram_tester.ExpectUniqueSample(
        "ResourceLoadingHints.ResourceLoadingBlocked",
        !test.allow_load_expected, 2);
    if (!test.allow_load_expected) {
      histogram_tester.ExpectUniqueSample(
          "ResourceLoadingHints.ResourceLoadingBlocked.ResourceLoadPriority."
          "Blocked",
          ResourceLoadPriority::kHighest, 2);
      histogram_tester.ExpectTotalCount(
          "ResourceLoadingHints.ResourceLoadingBlocked.ResourceLoadPriority."
          "Allowed",
          0);
    } else {
      histogram_tester.ExpectTotalCount(
          "ResourceLoadingHints.ResourceLoadingBlocked.ResourceLoadPriority."
          "Blocked",
          0);
      histogram_tester.ExpectUniqueSample(
          "ResourceLoadingHints.ResourceLoadingBlocked.ResourceLoadPriority."
          "Allowed",
          ResourceLoadPriority::kHighest, 2);
    }
  }
}

TEST_F(PreviewsResourceLoadingHintsTest, MultiplePatterns) {
  Vector<WTF::String> subresources_to_block;
  subresources_to_block.push_back(".example1.com/foo.jpg");
  subresources_to_block.push_back(".example1.com/bar.jpg");
  subresources_to_block.push_back(".example2.com/baz.jpg");

  PreviewsResourceLoadingHints* hints = PreviewsResourceLoadingHints::Create(
      dummy_page_holder_->GetDocument(), ukm::UkmRecorder::GetNewSourceID(),
      subresources_to_block);

  const struct {
    KURL url;
    bool allow_load_expected;
  } tests[] = {
      {KURL("https://www.example1.com/"), true},
      {KURL("https://www.example1.com/foo.js"), true},
      {KURL("https://www.example1.com/foo.jpg"), false},
      {KURL("https://www.example1.com/pages/foo.jpg"), true},
      {KURL("https://www.example1.com/foobar.jpg"), true},
      {KURL("https://www.example1.com/barfoo.jpg"), true},
      {KURL("http://www.example1.com/foo.jpg"), false},
      {KURL("http://www.example1.com/bar.jpg"), false},
      {KURL("http://www.example2.com/baz.jpg"), false},
      {KURL("http://www.example2.com/pages/baz.jpg"), true},
      {KURL("http://www.example2.com/baz.html"), true},
  };

  for (const auto& test : tests) {
    EXPECT_EQ(test.allow_load_expected,
              hints->AllowLoad(ResourceType::kScript, test.url,
                               ResourceLoadPriority::kHighest));
  }
}

TEST_F(PreviewsResourceLoadingHintsTest, OnePatternHistogramChecker) {
  Vector<WTF::String> subresources_to_block;
  subresources_to_block.push_back("foo.jpg");

  PreviewsResourceLoadingHints* hints = PreviewsResourceLoadingHints::Create(
      dummy_page_holder_->GetDocument(), ukm::UkmRecorder::GetNewSourceID(),
      subresources_to_block);

  const struct {
    KURL url;
    bool allow_load_expected;
    ResourceLoadPriority resource_load_priority;
  } tests[] = {
      {KURL("https://www.example.com/foo.js"), true,
       ResourceLoadPriority::kLow},
      {KURL("https://www.example.com/foo.jpg"), false,
       ResourceLoadPriority::kLow},
      {KURL("https://www.example.com/foo.js"), true,
       ResourceLoadPriority::kMedium},
      {KURL("https://www.example.com/foo.jpg"), false,
       ResourceLoadPriority::kMedium},
  };

  for (const auto& test : tests) {
    base::HistogramTester histogram_tester;
    EXPECT_EQ(test.allow_load_expected,
              hints->AllowLoad(ResourceType::kScript, test.url,
                               test.resource_load_priority));
    histogram_tester.ExpectUniqueSample(
        "ResourceLoadingHints.ResourceLoadingBlocked",
        !test.allow_load_expected, 1);
    if (!test.allow_load_expected) {
      histogram_tester.ExpectUniqueSample(
          "ResourceLoadingHints.ResourceLoadingBlocked.ResourceLoadPriority."
          "Blocked",
          test.resource_load_priority, 1);
      histogram_tester.ExpectTotalCount(
          "ResourceLoadingHints.ResourceLoadingBlocked.ResourceLoadPriority."
          "Allowed",
          0);
    } else {
      histogram_tester.ExpectTotalCount(
          "ResourceLoadingHints.ResourceLoadingBlocked.ResourceLoadPriority."
          "Blocked",
          0);
      histogram_tester.ExpectUniqueSample(
          "ResourceLoadingHints.ResourceLoadingBlocked.ResourceLoadPriority."
          "Allowed",
          test.resource_load_priority, 1);
    }
  }
}

TEST_F(PreviewsResourceLoadingHintsTest, MultiplePatternUKMChecker) {
  Vector<WTF::String> subresources_to_block;
  subresources_to_block.push_back(".example1.com/low_1.jpg");
  subresources_to_block.push_back(".example1.com/very_low_1.jpg");
  subresources_to_block.push_back(".example1.com/very_high_1.jpg");
  subresources_to_block.push_back(".example1.com/medium_1_and_medium_4.jpg");
  subresources_to_block.push_back(".example1.com/unused_1.jpg");
  subresources_to_block.push_back(".example2.com/medium_2.jpg");
  subresources_to_block.push_back(".example2.com/unused_2.jpg");
  subresources_to_block.push_back(".example3.com/unused_3.jpg");
  subresources_to_block.push_back(".example3.com/very_low_2_and_medium_3.jpg");

  PreviewsResourceLoadingHints* hints = PreviewsResourceLoadingHints::Create(
      dummy_page_holder_->GetDocument(), ukm::UkmRecorder::GetNewSourceID(),
      subresources_to_block);

  const struct {
    KURL url;
    ResourceLoadPriority resource_load_priority;
  } resources_to_load[] = {
      {KURL("https://www.example1.com/"), ResourceLoadPriority::kHigh},
      {KURL("https://www.example1.com/foo.js"), ResourceLoadPriority::kLow},
      {KURL("https://www.example1.com/very_low_1.jpg"),
       ResourceLoadPriority::kVeryLow},
      {KURL("https://www.example1.com/low_1.jpg"), ResourceLoadPriority::kLow},
      {KURL("https://www.example1.com/very_high_1.jpg"),
       ResourceLoadPriority::kVeryHigh},
      {KURL("https://www.example1.com/pages/foo.jpg"),
       ResourceLoadPriority::kVeryLow},
      {KURL("https://www.example1.com/foobar.jpg"),
       ResourceLoadPriority::kVeryHigh},
      {KURL("https://www.example1.com/barfoo.jpg"),
       ResourceLoadPriority::kVeryHigh},
      {KURL("http://www.example1.com/foo.jpg"), ResourceLoadPriority::kLow},
      {KURL("http://www.example1.com/medium_1_and_medium_4.jpg"),
       ResourceLoadPriority::kMedium},
      {KURL("http://www.example2.com/medium_2.jpg"),
       ResourceLoadPriority::kMedium},
      {KURL("http://www.example2.com/pages/baz.jpg"),
       ResourceLoadPriority::kLow},
      {KURL("http://www.example2.com/baz.html"),
       ResourceLoadPriority::kVeryHigh},
      {KURL("http://www.example3.com/very_low_2_and_medium_3.jpg"),
       ResourceLoadPriority::kVeryLow},
      {KURL("http://www.example3.com/very_low_2_and_medium_3.jpg"),
       ResourceLoadPriority::kMedium},
      {KURL("http://www.example1.com/medium_1_and_medium_4.jpg"),
       ResourceLoadPriority::kMedium},
  };

  for (const auto& resource_to_load : resources_to_load) {
    hints->AllowLoad(ResourceType::kScript, resource_to_load.url,
                     resource_to_load.resource_load_priority);
  }

  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  hints->RecordUKM(&test_ukm_recorder);

  using UkmEntry = ukm::builders::PreviewsResourceLoadingHints;
  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0];
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmEntry::kpatterns_to_block_totalName, 9);
  test_ukm_recorder.ExpectEntryMetric(entry,
                                      UkmEntry::kpatterns_to_block_usedName, 6);
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmEntry::kblocked_very_low_priorityName, 2);
  test_ukm_recorder.ExpectEntryMetric(entry,
                                      UkmEntry::kblocked_low_priorityName, 1);
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmEntry::kblocked_medium_priorityName, 4);
  test_ukm_recorder.ExpectEntryMetric(entry,
                                      UkmEntry::kblocked_high_priorityName, 0);
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmEntry::kblocked_very_high_priorityName, 1);
}

// Test class that overrides field trial so that resource blocking hints apply
// to images as well.
class PreviewsResourceLoadingHintsTestBlockImages
    : public PreviewsResourceLoadingHintsTest {
 public:
  PreviewsResourceLoadingHintsTestBlockImages() = default;

  void SetUp() override {
    std::map<std::string, std::string> feature_parameters;
    feature_parameters["block_resource_type_1"] = "true";

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kPreviewsResourceLoadingHintsSpecificResourceTypes,
        feature_parameters);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PreviewsResourceLoadingHintsTestBlockImages,
       OnePatternWithResourceSubtype) {
  Vector<WTF::String> subresources_to_block;
  subresources_to_block.push_back("foo.jpg");

  PreviewsResourceLoadingHints* hints = PreviewsResourceLoadingHints::Create(
      dummy_page_holder_->GetDocument(), ukm::UkmRecorder::GetNewSourceID(),
      subresources_to_block);

  const struct {
    KURL url;
    bool allow_load_expected;
  } tests[] = {
      {KURL("https://www.example.com/"), true},
      {KURL("https://www.example.com/foo.js"), true},
      {KURL("https://www.example.com/foo.jpg"), false},
      {KURL("https://www.example.com/pages/foo.jpg"), false},
      {KURL("https://www.example.com/foobar.jpg"), true},
      {KURL("https://www.example.com/barfoo.jpg"), false},
      {KURL("http://www.example.com/foo.jpg"), false},
      {KURL("http://www.example.com/foo.jpg?q=alpha"), false},
      {KURL("http://www.example.com/bar.jpg?q=foo.jpg"), true},
      {KURL("http://www.example.com/bar.jpg?q=foo.jpg#foo.jpg"), true},
  };

  for (const auto& test : tests) {
    // By default, resource blocking hints do not apply to fonts.
    EXPECT_TRUE(hints->AllowLoad(ResourceType::kFont, test.url,
                                 ResourceLoadPriority::kHighest));
    // Feature override should cause resource blocking hints to apply to images.
    EXPECT_EQ(test.allow_load_expected,
              hints->AllowLoad(ResourceType::kImage, test.url,
                               ResourceLoadPriority::kHighest));
    EXPECT_EQ(test.allow_load_expected,
              hints->AllowLoad(ResourceType::kScript, test.url,
                               ResourceLoadPriority::kHighest));
  }
}

// Test class that overrides field trial so that resource blocking hints do not
// apply to CSS.
class PreviewsResourceLoadingHintsTestAllowCSS
    : public PreviewsResourceLoadingHintsTestBlockImages {
 public:
  PreviewsResourceLoadingHintsTestAllowCSS() = default;

  void SetUp() override {
    std::map<std::string, std::string> feature_parameters;
    feature_parameters["block_resource_type_2"] = "false";

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kPreviewsResourceLoadingHintsSpecificResourceTypes,
        feature_parameters);
  }
};

TEST_F(PreviewsResourceLoadingHintsTestAllowCSS,
       OnePatternWithResourceSubtype) {
  Vector<WTF::String> subresources_to_block;
  subresources_to_block.push_back("foo.jpg");

  PreviewsResourceLoadingHints* hints = PreviewsResourceLoadingHints::Create(
      dummy_page_holder_->GetDocument(), ukm::UkmRecorder::GetNewSourceID(),
      subresources_to_block);

  const struct {
    KURL url;
    bool allow_load_expected;
  } tests[] = {
      {KURL("https://www.example.com/"), true},
      {KURL("https://www.example.com/foo.js"), true},
      {KURL("https://www.example.com/foo.jpg"), false},
      {KURL("https://www.example.com/pages/foo.jpg"), false},
      {KURL("https://www.example.com/foobar.jpg"), true},
      {KURL("https://www.example.com/barfoo.jpg"), false},
      {KURL("http://www.example.com/foo.jpg"), false},
      {KURL("http://www.example.com/foo.jpg?q=alpha"), false},
      {KURL("http://www.example.com/bar.jpg?q=foo.jpg"), true},
      {KURL("http://www.example.com/bar.jpg?q=foo.jpg#foo.jpg"), true},
  };

  for (const auto& test : tests) {
    // Feature override should cause resource blocking hints to apply to only
    // scripts.
    EXPECT_TRUE(hints->AllowLoad(ResourceType::kFont, test.url,
                                 ResourceLoadPriority::kHighest));
    EXPECT_TRUE(hints->AllowLoad(ResourceType::kImage, test.url,
                                 ResourceLoadPriority::kHighest));
    EXPECT_TRUE(hints->AllowLoad(ResourceType::kCSSStyleSheet, test.url,
                                 ResourceLoadPriority::kHighest));
    EXPECT_EQ(test.allow_load_expected,
              hints->AllowLoad(ResourceType::kScript, test.url,
                               ResourceLoadPriority::kHighest));
  }
}

}  // namespace

}  // namespace blink
