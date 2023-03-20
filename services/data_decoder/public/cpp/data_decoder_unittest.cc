// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/data_decoder.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/rust_buildflags.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
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

TEST_F(DataDecoderTest, Reuse) {
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

TEST_F(DataDecoderTest, Isolation) {
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

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(BUILD_RUST_JSON_READER)

class DataDecoderMultiThreadTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(DataDecoderMultiThreadTest, JSONDecode) {
  // Test basic JSON decoding. We test only on Android or if Rust
  // is enabled, because otherwise this would result in spawning
  // a process.
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
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->is_list());
  base::Value::List& list = result->GetList();
  ASSERT_EQ(1u, list.size());
  EXPECT_TRUE(list[0].is_double());
  EXPECT_EQ(122.416294033786585, list[0].GetDouble());
}

#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(BUILD_RUST_JSON_READER)

}  // namespace data_decoder
