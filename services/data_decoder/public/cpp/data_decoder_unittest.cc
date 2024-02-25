// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/data_decoder.h"

#include <memory>

#include "base/features.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/rust_buildflags.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/data_decoder/public/mojom/cbor_parser.mojom.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_decoder {

class DataDecoderTest : public ::testing::Test {
 public:
  test::InProcessDataDecoder& service() { return in_process_data_decoder_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(DataDecoderTest, ReuseJson) {
  // Verify that a single DataDecoder with concurrent interface connections will
  // only use one service instance.

  DataDecoder decoder;
  mojo::Remote<mojom::JsonParser> parser1;
  decoder.GetService()->BindJsonParser(parser1.BindNewPipeAndPassReceiver());
  parser1.FlushForTesting();
  EXPECT_TRUE(parser1.is_connected());
  EXPECT_EQ(1u, service().receivers().size());

  mojo::Remote<mojom::JsonParser> parser2;
  decoder.GetService()->BindJsonParser(parser2.BindNewPipeAndPassReceiver());
  parser2.FlushForTesting();
  EXPECT_TRUE(parser2.is_connected());
  EXPECT_TRUE(parser1.is_connected());
  EXPECT_EQ(1u, service().receivers().size());
}

TEST_F(DataDecoderTest, IsolationJson) {
  // Verify that separate DataDecoder instances make separate connections to the
  // service.

  DataDecoder decoder1;
  mojo::Remote<mojom::JsonParser> parser1;
  decoder1.GetService()->BindJsonParser(parser1.BindNewPipeAndPassReceiver());
  parser1.FlushForTesting();
  EXPECT_TRUE(parser1.is_connected());
  EXPECT_EQ(1u, service().receivers().size());

  DataDecoder decoder2;
  mojo::Remote<mojom::JsonParser> parser2;
  decoder2.GetService()->BindJsonParser(parser2.BindNewPipeAndPassReceiver());
  parser2.FlushForTesting();
  EXPECT_TRUE(parser2.is_connected());
  EXPECT_EQ(2u, service().receivers().size());
}

TEST_F(DataDecoderTest, ReuseCbor) {
  // Verify that a single DataDecoder with concurrent interface connections will
  // only use one service instance.
  DataDecoder decoder;
  mojo::Remote<mojom::CborParser> parser1;
  decoder.GetService()->BindCborParser(parser1.BindNewPipeAndPassReceiver());
  parser1.FlushForTesting();
  EXPECT_TRUE(parser1.is_connected());
  EXPECT_EQ(1u, service().receivers().size());

  mojo::Remote<mojom::CborParser> parser2;
  decoder.GetService()->BindCborParser(parser2.BindNewPipeAndPassReceiver());
  parser2.FlushForTesting();
  EXPECT_TRUE(parser1.is_connected());
  EXPECT_TRUE(parser2.is_connected());
  EXPECT_EQ(1u, service().receivers().size());
}

TEST_F(DataDecoderTest, IsolationCbor) {
  // Verify that separate DataDecoder instances make separate connections to the
  // service.
  DataDecoder decoder1;
  mojo::Remote<mojom::CborParser> parser1;
  decoder1.GetService()->BindCborParser(parser1.BindNewPipeAndPassReceiver());
  parser1.FlushForTesting();
  EXPECT_TRUE(parser1.is_connected());
  EXPECT_EQ(1u, service().receivers().size());

  DataDecoder decoder2;
  mojo::Remote<mojom::CborParser> parser2;
  decoder2.GetService()->BindCborParser(parser2.BindNewPipeAndPassReceiver());
  parser2.FlushForTesting();
  EXPECT_TRUE(parser2.is_connected());
  EXPECT_EQ(2u, service().receivers().size());
}

TEST_F(DataDecoderTest, ParseCborToInteger) {
  base::RunLoop run_loop;
  DataDecoder decoder;
  DataDecoder::ValueOrError result;
  // 100
  std::vector<uint8_t> input = {0x18, 0x64};

  decoder.ParseCborIsolated(
      input,
      base::BindLambdaForTesting(
          [&run_loop, &result](DataDecoder::ValueOrError value_or_error) {
            result = std::move(value_or_error);
            run_loop.Quit();
          }));
  run_loop.Run();

  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->is_int());
  ASSERT_EQ(result->GetInt(), 100);
}

TEST_F(DataDecoderTest, ParseCborAndFailed) {
  base::RunLoop run_loop;
  DataDecoder decoder;
  DataDecoder::ValueOrError result;
  // Null
  std::vector<uint8_t> input = {0xF6};

  decoder.ParseCborIsolated(
      input,
      base::BindLambdaForTesting(
          [&run_loop, &result](DataDecoder::ValueOrError value_or_error) {
            result = std::move(value_or_error);
            run_loop.Quit();
          }));
  run_loop.Run();

  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error(), "Error unexpected CBOR value.");
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(BUILD_RUST_JSON_READER)

class DataDecoderMultiThreadTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

TEST_F(DataDecoderMultiThreadTest, JSONDecode) {
  // Test basic JSON decoding. We test only on Android or if Rust
  // is enabled, because otherwise this would result in spawning
  // a process.
#if !BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(base::features::kUseRustJsonParser);
#endif  // !BUILDFLAG(IS_ANDROID)

  base::RunLoop run_loop;
  DataDecoder decoder;
  DataDecoder::ValueOrError result;
  decoder.ParseJson(
      // The magic 122.416294033786585 number comes from
      // https://github.com/serde-rs/json/issues/707
      "[ 122.416294033786585 ]",
      base::BindLambdaForTesting(
          [&run_loop, &result](DataDecoder::ValueOrError value_or_error) {
            result = std::move(value_or_error);
            run_loop.Quit();
          }));
  run_loop.Run();
  histogram_tester_.ExpectTotalCount("Security.DataDecoder.Json.DecodingTime",
                                     1);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->is_list());
  base::Value::List& list = result->GetList();
  ASSERT_EQ(1u, list.size());
  EXPECT_TRUE(list[0].is_double());
  EXPECT_EQ(122.416294033786585, list[0].GetDouble());
}

#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(BUILD_RUST_JSON_READER)

}  // namespace data_decoder
