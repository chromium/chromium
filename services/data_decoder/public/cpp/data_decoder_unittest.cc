// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/data_decoder.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
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

}  // namespace data_decoder
