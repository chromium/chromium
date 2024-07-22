// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_for_streaming.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/v8_compile_hints_histograms.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_local_compile_hints_consumer.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink::v8_compile_hints {

class CompileHintsForStreamingTest : public ::testing::Test {
 public:
  ~CompileHintsForStreamingTest() override {
    // Disable kProduceCompileHints2 not to randomly produce compile hints.
    scoped_feature_list_.InitAndDisableFeature(features::kProduceCompileHints2);
  }

  CompileHintsForStreamingTest(const CompileHintsForStreamingTest&) = delete;
  CompileHintsForStreamingTest& operator=(const CompileHintsForStreamingTest&) =
      delete;

 protected:
  CompileHintsForStreamingTest() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  test::TaskEnvironment task_environment_;
};

TEST_F(CompileHintsForStreamingTest, NoCrowdsourcedNoLocalNoMagicComment1) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kLocalCompileHints);
  auto builder = CompileHintsForStreaming::Builder(
      /*crowdsourced_compile_hints_producer=*/nullptr,
      /*crowdsourced_compile_hints_consumer=*/nullptr,
      KURL("https://example.com/"),
      /*v8_compile_hints_magic_comment_runtime_enabled=*/false);
  base::HistogramTester histogram_tester;
  auto compile_hints_for_streaming = std::move(builder).Build(
      /*cached_metadata=*/nullptr, /*has_hot_timestamp=*/true);
  histogram_tester.ExpectUniqueSample(kStatusHistogram,
                                      Status::kNoCompileHintsStreaming, 1);
  ASSERT_TRUE(compile_hints_for_streaming);
  EXPECT_EQ(v8::ScriptCompiler::kNoCompileOptions,
            compile_hints_for_streaming->compile_options());
}

TEST_F(CompileHintsForStreamingTest, NoCrowdsourcedNoLocalNoMagicComment2) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kLocalCompileHints);
  auto builder = CompileHintsForStreaming::Builder(
      /*crowdsourced_compile_hints_producer=*/nullptr,
      /*crowdsourced_compile_hints_consumer=*/nullptr,
      KURL("https://example.com/"),
      /*v8_compile_hints_magic_comment_runtime_enabled=*/true);
  base::HistogramTester histogram_tester;
  auto compile_hints_for_streaming = std::move(builder).Build(
      /*cached_metadata=*/nullptr, /*has_hot_timestamp=*/false);
  histogram_tester.ExpectUniqueSample(kStatusHistogram,
                                      Status::kNoCompileHintsStreaming, 1);
  ASSERT_TRUE(compile_hints_for_streaming);
  EXPECT_EQ(v8::ScriptCompiler::kNoCompileOptions,
            compile_hints_for_streaming->compile_options());
}

TEST_F(CompileHintsForStreamingTest, NoCrowdsourcedNoLocalButMagicComment) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kLocalCompileHints);
  auto builder = CompileHintsForStreaming::Builder(
      /*crowdsourced_compile_hints_producer=*/nullptr,
      /*crowdsourced_compile_hints_consumer=*/nullptr,
      KURL("https://example.com/"),
      /*v8_compile_hints_magic_comment_runtime_enabled=*/true);
  base::HistogramTester histogram_tester;
  auto compile_hints_for_streaming = std::move(builder).Build(
      /*cached_metadata=*/nullptr, /*has_hot_timestamp=*/true);
  histogram_tester.ExpectUniqueSample(kStatusHistogram,
                                      Status::kNoCompileHintsStreaming, 1);
  ASSERT_TRUE(compile_hints_for_streaming);
  EXPECT_EQ(v8::ScriptCompiler::kFollowCompileHintsMagicComment,
            compile_hints_for_streaming->compile_options());
}

TEST_F(CompileHintsForStreamingTest, ProduceLocalNoMagicComment) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kLocalCompileHints);
  auto builder = CompileHintsForStreaming::Builder(
      /*crowdsourced_compile_hints_producer=*/nullptr,
      /*crowdsourced_compile_hints_consumer=*/nullptr,
      KURL("https://example.com/"),
      /*v8_compile_hints_magic_comment_runtime_enabled=*/true);
  base::HistogramTester histogram_tester;
  auto compile_hints_for_streaming = std::move(builder).Build(
      /*cached_metadata=*/nullptr, /*has_hot_timestamp=*/false);
  histogram_tester.ExpectUniqueSample(kStatusHistogram,
                                      Status::kProduceCompileHintsStreaming, 1);
  ASSERT_TRUE(compile_hints_for_streaming);
  EXPECT_EQ(compile_hints_for_streaming->compile_options(),
            v8::ScriptCompiler::kProduceCompileHints);
  EXPECT_FALSE(compile_hints_for_streaming->GetCompileHintCallback());
  EXPECT_FALSE(compile_hints_for_streaming->GetCompileHintCallbackData());
}

TEST_F(CompileHintsForStreamingTest, ConsumeLocalNoMagicComment) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kLocalCompileHints);
  auto builder = CompileHintsForStreaming::Builder(
      /*crowdsourced_compile_hints_producer=*/nullptr,
      /*crowdsourced_compile_hints_consumer=*/nullptr,
      KURL("https://example.com/"),
      /*v8_compile_hints_magic_comment_runtime_enabled=*/false);
  const uint32_t kCacheTagCompileHints = 2;
  const uint64_t kDummyTag = 1;
  Vector<uint8_t> dummy_data(100);
  scoped_refptr<CachedMetadata> metadata = CachedMetadata::Create(
      kCacheTagCompileHints, dummy_data.data(), dummy_data.size(), kDummyTag);
  base::HistogramTester histogram_tester;
  auto compile_hints_for_streaming =
      std::move(builder).Build(std::move(metadata), /*has_hot_timestamp=*/true);
  histogram_tester.ExpectUniqueSample(
      kStatusHistogram, Status::kConsumeLocalCompileHintsStreaming, 1);
  ASSERT_TRUE(compile_hints_for_streaming);
  EXPECT_EQ(compile_hints_for_streaming->compile_options(),
            v8::ScriptCompiler::kConsumeCompileHints);
  EXPECT_EQ(
      compile_hints_for_streaming->GetCompileHintCallback(),
      v8::CompileHintCallback(V8LocalCompileHintsConsumer::GetCompileHint));
  EXPECT_TRUE(compile_hints_for_streaming->GetCompileHintCallbackData());
}

TEST_F(CompileHintsForStreamingTest, ConsumeLocalMagicComment) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kLocalCompileHints);
  auto builder = CompileHintsForStreaming::Builder(
      /*crowdsourced_compile_hints_producer=*/nullptr,
      /*crowdsourced_compile_hints_consumer=*/nullptr,
      KURL("https://example.com/"),
      /*v8_compile_hints_magic_comment_runtime_enabled=*/true);
  const uint32_t kCacheTagCompileHints = 2;
  const uint64_t kDummyTag = 1;
  Vector<uint8_t> dummy_data(100);
  scoped_refptr<CachedMetadata> metadata = CachedMetadata::Create(
      kCacheTagCompileHints, dummy_data.data(), dummy_data.size(), kDummyTag);
  base::HistogramTester histogram_tester;
  auto compile_hints_for_streaming =
      std::move(builder).Build(std::move(metadata), /*has_hot_timestamp=*/true);
  histogram_tester.ExpectUniqueSample(
      kStatusHistogram, Status::kConsumeLocalCompileHintsStreaming, 1);
  ASSERT_TRUE(compile_hints_for_streaming);
  EXPECT_EQ(compile_hints_for_streaming->compile_options(),
            v8::ScriptCompiler::kConsumeCompileHints |
                v8::ScriptCompiler::kFollowCompileHintsMagicComment);
  EXPECT_EQ(
      compile_hints_for_streaming->GetCompileHintCallback(),
      v8::CompileHintCallback(V8LocalCompileHintsConsumer::GetCompileHint));
  EXPECT_TRUE(compile_hints_for_streaming->GetCompileHintCallbackData());
}

TEST_F(CompileHintsForStreamingTest,
       FailedToConsumeLocalWrongSizeNoMagicComment) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kLocalCompileHints);
  base::HistogramTester histogram_tester;
  auto builder = CompileHintsForStreaming::Builder(
      /*crowdsourced_compile_hints_producer=*/nullptr,
      /*crowdsourced_compile_hints_consumer=*/nullptr,
      KURL("https://example.com/"),
      /*v8_compile_hints_magic_comment_runtime_enabled=*/false);
  const uint32_t kCacheTagCompileHints = 2;
  const uint64_t kDummyTag = 1;
  Vector<uint8_t> dummy_data(1);  // Too small.
  scoped_refptr<CachedMetadata> metadata = CachedMetadata::Create(
      kCacheTagCompileHints, dummy_data.data(), dummy_data.size(), kDummyTag);
  auto compile_hints_for_streaming =
      std::move(builder).Build(std::move(metadata), /*has_hot_timestamp=*/true);
  EXPECT_TRUE(compile_hints_for_streaming);
  EXPECT_EQ(v8::ScriptCompiler::kNoCompileOptions,
            compile_hints_for_streaming->compile_options());
}

TEST_F(CompileHintsForStreamingTest,
       FailedToConsumeLocalWrongSizeMagicComment) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kLocalCompileHints);
  base::HistogramTester histogram_tester;
  auto builder = CompileHintsForStreaming::Builder(
      /*crowdsourced_compile_hints_producer=*/nullptr,
      /*crowdsourced_compile_hints_consumer=*/nullptr,
      KURL("https://example.com/"),
      /*v8_compile_hints_magic_comment_runtime_enabled=*/true);
  const uint32_t kCacheTagCompileHints = 2;
  const uint64_t kDummyTag = 1;
  Vector<uint8_t> dummy_data(1);  // Too small.
  scoped_refptr<CachedMetadata> metadata = CachedMetadata::Create(
      kCacheTagCompileHints, dummy_data.data(), dummy_data.size(), kDummyTag);
  auto compile_hints_for_streaming =
      std::move(builder).Build(std::move(metadata), /*has_hot_timestamp=*/true);
  EXPECT_TRUE(compile_hints_for_streaming);
  EXPECT_EQ(v8::ScriptCompiler::kNoCompileOptions |
                v8::ScriptCompiler::kFollowCompileHintsMagicComment,
            compile_hints_for_streaming->compile_options());
}

TEST_F(CompileHintsForStreamingTest, ConsumeCrowdsourcedHintNoMagicComment) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  Page* page = web_view_helper.GetWebView()->GetPage();

  auto* crowdsourced_compile_hints_producer =
      &page->GetV8CrowdsourcedCompileHintsProducer();
  auto* crowdsourced_compile_hints_consumer =
      &page->GetV8CrowdsourcedCompileHintsConsumer();
  Vector<int64_t> dummy_data(kBloomFilterInt32Count / 2);
  crowdsourced_compile_hints_consumer->SetData(dummy_data.data(),
                                               dummy_data.size());

  auto builder = CompileHintsForStreaming::Builder(
      crowdsourced_compile_hints_producer, crowdsourced_compile_hints_consumer,
      KURL("https://example.com/"),
      /*v8_compile_hints_magic_comment_runtime_enabled=*/true);

  base::HistogramTester histogram_tester;
  auto compile_hints_for_streaming = std::move(builder).Build(
      /*cached_metadata=*/nullptr, /*has_hot_timestamp=*/false);
  histogram_tester.ExpectUniqueSample(
      kStatusHistogram, Status::kConsumeCrowdsourcedCompileHintsStreaming, 1);

  ASSERT_TRUE(compile_hints_for_streaming);
  EXPECT_EQ(compile_hints_for_streaming->compile_options(),
            v8::ScriptCompiler::kConsumeCompileHints);
  EXPECT_EQ(compile_hints_for_streaming->GetCompileHintCallback(),
            &V8CrowdsourcedCompileHintsConsumer::CompileHintCallback);
  EXPECT_TRUE(compile_hints_for_streaming->GetCompileHintCallbackData());
}

TEST_F(CompileHintsForStreamingTest, PreferCrowdsourcedHints) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  Page* page = web_view_helper.GetWebView()->GetPage();

  auto* crowdsourced_compile_hints_producer =
      &page->GetV8CrowdsourcedCompileHintsProducer();
  auto* crowdsourced_compile_hints_consumer =
      &page->GetV8CrowdsourcedCompileHintsConsumer();
  Vector<int64_t> dummy_data(kBloomFilterInt32Count / 2);
  crowdsourced_compile_hints_consumer->SetData(dummy_data.data(),
                                               dummy_data.size());

  const uint32_t kCacheTagCompileHints = 2;
  const uint64_t kDummyTag = 1;
  Vector<uint8_t> local_dummy_data(100);
  scoped_refptr<CachedMetadata> metadata =
      CachedMetadata::Create(kCacheTagCompileHints, local_dummy_data.data(),
                             local_dummy_data.size(), kDummyTag);

  base::HistogramTester histogram_tester;
  auto builder = CompileHintsForStreaming::Builder(
      crowdsourced_compile_hints_producer, crowdsourced_compile_hints_consumer,
      KURL("https://example.com/"),
      /*v8_compile_hints_magic_comment_runtime_enabled=*/true);

  auto compile_hints_for_streaming =
      std::move(builder).Build(metadata, /*has_hot_timestamp=*/true);

  // We prefer crowdsourced hints over local hints, if both are available.
  histogram_tester.ExpectUniqueSample(
      kStatusHistogram, Status::kConsumeCrowdsourcedCompileHintsStreaming, 1);

  ASSERT_TRUE(compile_hints_for_streaming);
  EXPECT_EQ(compile_hints_for_streaming->compile_options(),
            v8::ScriptCompiler::kConsumeCompileHints |
                v8::ScriptCompiler::kFollowCompileHintsMagicComment);
  EXPECT_EQ(compile_hints_for_streaming->GetCompileHintCallback(),
            &V8CrowdsourcedCompileHintsConsumer::CompileHintCallback);
  EXPECT_TRUE(compile_hints_for_streaming->GetCompileHintCallbackData());
}

TEST_F(CompileHintsForStreamingTest, ProduceCrowdsourcedHintNoMagicComment) {
  // Disable local compile hints, since otherwise we'd always produce compile
  // hints anyway, and couldn't test producing compile hints for crowdsourcing
  // purposes.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kForceProduceCompileHints},
                                       {features::kLocalCompileHints});

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  Page* page = web_view_helper.GetWebView()->GetPage();

  auto* crowdsourced_compile_hints_producer =
      &page->GetV8CrowdsourcedCompileHintsProducer();
  auto* crowdsourced_compile_hints_consumer =
      &page->GetV8CrowdsourcedCompileHintsConsumer();

  auto builder = CompileHintsForStreaming::Builder(
      crowdsourced_compile_hints_producer, crowdsourced_compile_hints_consumer,
      KURL("https://example.com/"),
      /*v8_compile_hints_magic_comment_runtime_enabled=*/true);

  base::HistogramTester histogram_tester;
  auto compile_hints_for_streaming = std::move(builder).Build(
      /*cached_metadata=*/nullptr, /*has_hot_timestamp=*/false);
  ASSERT_TRUE(compile_hints_for_streaming);
  EXPECT_FALSE(compile_hints_for_streaming->GetCompileHintCallback());
  EXPECT_FALSE(compile_hints_for_streaming->GetCompileHintCallbackData());

#if BUILDFLAG(PRODUCE_V8_COMPILE_HINTS)
  histogram_tester.ExpectUniqueSample(kStatusHistogram,
                                      Status::kProduceCompileHintsStreaming, 1);
  EXPECT_EQ(compile_hints_for_streaming->compile_options(),
            v8::ScriptCompiler::kProduceCompileHints);
#else  // BUILDFLAG(PRODUCE_V8_COMPILE_HINTS)
  histogram_tester.ExpectUniqueSample(kStatusHistogram,
                                      Status::kNoCompileHintsStreaming, 1);
  EXPECT_EQ(compile_hints_for_streaming->compile_options(),
            v8::ScriptCompiler::kNoCompileOptions);
#endif
}

TEST_F(CompileHintsForStreamingTest, ProduceCrowdsourcedHintMagicComment) {
  // Disable local compile hints, since otherwise we'd always produce compile
  // hints anyway, and couldn't test producing compile hints for crowdsourcing
  // purposes.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kForceProduceCompileHints},
                                       {features::kLocalCompileHints});

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  Page* page = web_view_helper.GetWebView()->GetPage();

  auto* crowdsourced_compile_hints_producer =
      &page->GetV8CrowdsourcedCompileHintsProducer();
  auto* crowdsourced_compile_hints_consumer =
      &page->GetV8CrowdsourcedCompileHintsConsumer();

  auto builder = CompileHintsForStreaming::Builder(
      crowdsourced_compile_hints_producer, crowdsourced_compile_hints_consumer,
      KURL("https://example.com/"),
      /*v8_compile_hints_magic_comment_runtime_enabled=*/true);

  base::HistogramTester histogram_tester;
  auto compile_hints_for_streaming = std::move(builder).Build(
      /*cached_metadata=*/nullptr, /*has_hot_timestamp=*/true);
  ASSERT_TRUE(compile_hints_for_streaming);
  EXPECT_FALSE(compile_hints_for_streaming->GetCompileHintCallback());
  EXPECT_FALSE(compile_hints_for_streaming->GetCompileHintCallbackData());

#if BUILDFLAG(PRODUCE_V8_COMPILE_HINTS)
  histogram_tester.ExpectUniqueSample(kStatusHistogram,
                                      Status::kProduceCompileHintsStreaming, 1);
  EXPECT_EQ(compile_hints_for_streaming->compile_options(),
            v8::ScriptCompiler::kProduceCompileHints |
                v8::ScriptCompiler::kFollowCompileHintsMagicComment);
#else  // BUILDFLAG(PRODUCE_V8_COMPILE_HINTS)
  histogram_tester.ExpectUniqueSample(kStatusHistogram,
                                      Status::kNoCompileHintsStreaming, 1);
  EXPECT_EQ(compile_hints_for_streaming->compile_options(),
            v8::ScriptCompiler::kFollowCompileHintsMagicComment);
#endif
}

}  // namespace blink::v8_compile_hints
