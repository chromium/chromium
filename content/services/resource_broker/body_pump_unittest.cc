// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/resource_broker/body_pump.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "crypto/hash.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_broker {

namespace {

// Reads everything written to a consumer handle, on the current sequence.
class Drainer : public mojo::DataPipeDrainer::Client {
 public:
  explicit Drainer(mojo::ScopedDataPipeConsumerHandle consumer)
      : drainer_(this, std::move(consumer)) {}

  const std::string& data() const { return data_; }
  bool complete() const { return complete_; }

 private:
  void OnDataAvailable(base::span<const uint8_t> data) override {
    data_.append(base::as_string_view(data));
  }
  void OnDataComplete() override { complete_ = true; }

  std::string data_;
  bool complete_ = false;
  mojo::DataPipeDrainer drainer_;
};

class BodyPumpTest : public testing::Test {
 protected:
  void CreatePipes(uint32_t capacity) { CreatePipes(capacity, capacity); }

  void CreatePipes(uint32_t upstream_capacity, uint32_t downstream_capacity) {
    ASSERT_EQ(mojo::CreateDataPipe(upstream_capacity, upstream_producer_,
                                   upstream_consumer_),
              MOJO_RESULT_OK);
    ASSERT_EQ(mojo::CreateDataPipe(downstream_capacity, downstream_producer_,
                                   downstream_consumer_),
              MOJO_RESULT_OK);
  }

  // Creates and starts a pump between the upstream consumer and the
  // downstream producer. The result is stored in `result_` on completion.
  void StartPump() {
    pump_ = std::make_unique<BodyPump>(
        std::move(upstream_consumer_), std::move(downstream_producer_),
        base::BindLambdaForTesting([&](const BodyPump::Result& result) {
          result_ = result;
          completion_loop_.Quit();
        }));
    pump_->Start();
  }

  void WaitForCompletion() { completion_loop_.Run(); }

  // Writes all of `data` to the upstream pipe and closes it, so the pump sees
  // the body followed by end of stream. Writing is asynchronous so that a body
  // larger than the pipes can be drained by the pump while it is written.
  void WriteUpstreamAndClose(std::string_view data) {
    upstream_writer_ =
        std::make_unique<mojo::DataPipeProducer>(std::move(upstream_producer_));
    upstream_writer_->Write(std::make_unique<mojo::StringDataSource>(
                                data, mojo::StringDataSource::AsyncWritingMode::
                                          STRING_STAYS_VALID_UNTIL_COMPLETION),
                            base::BindLambdaForTesting([&](MojoResult result) {
                              EXPECT_EQ(result, MOJO_RESULT_OK);
                              upstream_writer_.reset();
                            }));
  }

  base::test::TaskEnvironment task_environment_;
  mojo::ScopedDataPipeProducerHandle upstream_producer_;
  std::unique_ptr<mojo::DataPipeProducer> upstream_writer_;
  mojo::ScopedDataPipeConsumerHandle upstream_consumer_;
  mojo::ScopedDataPipeProducerHandle downstream_producer_;
  mojo::ScopedDataPipeConsumerHandle downstream_consumer_;
  std::unique_ptr<BodyPump> pump_;
  std::optional<BodyPump::Result> result_;
  base::RunLoop completion_loop_;
};

TEST_F(BodyPumpTest, CopiesAndHashesBody) {
  const std::string body = "Hello World! This is a body pump test.";
  ASSERT_NO_FATAL_FAILURE(CreatePipes(1024));
  StartPump();

  WriteUpstreamAndClose(body);
  WaitForCompletion();

  ASSERT_TRUE(result_);
  EXPECT_EQ(result_->net_error, net::OK);
  EXPECT_EQ(result_->body_size, body.size());
  EXPECT_EQ(result_->body_hash, crypto::hash::Sha256(body));

  Drainer drainer(std::move(downstream_consumer_));
  ASSERT_TRUE(base::test::RunUntil([&] { return drainer.complete(); }));
  EXPECT_EQ(drainer.data(), body);
}

TEST_F(BodyPumpTest, EmptyBody) {
  ASSERT_NO_FATAL_FAILURE(CreatePipes(1024));
  StartPump();

  upstream_producer_.reset();
  WaitForCompletion();

  ASSERT_TRUE(result_);
  EXPECT_EQ(result_->net_error, net::OK);
  EXPECT_EQ(result_->body_size, 0u);
  EXPECT_EQ(result_->body_hash, crypto::hash::Sha256(""));
  EXPECT_TRUE(result_->pump_cpu_time.is_zero());
}

// A body larger than both pipes forces the pump to wait for the reader
// repeatedly. The copy must still be complete and in order.
TEST_F(BodyPumpTest, BodyLargerThanPipes) {
  if (base::ThreadTicks::IsSupported()) {
    base::ThreadTicks::WaitUntilInitialized();
  }
  std::string body;
  for (int i = 0; i < 20000; ++i) {
    body += "0123456789";
  }
  ASSERT_NO_FATAL_FAILURE(CreatePipes(4096));
  StartPump();
  Drainer drainer(std::move(downstream_consumer_));

  WriteUpstreamAndClose(body);
  WaitForCompletion();
  ASSERT_TRUE(base::test::RunUntil([&] { return drainer.complete(); }));

  ASSERT_TRUE(result_);
  EXPECT_EQ(result_->net_error, net::OK);
  EXPECT_EQ(result_->body_size, body.size());
  EXPECT_EQ(result_->body_hash, crypto::hash::Sha256(body));
  EXPECT_TRUE(result_->duration.is_positive());
  EXPECT_GE(result_->pump_cpu_time, base::TimeDelta());
  EXPECT_EQ(drainer.data(), body);
}

TEST_F(BodyPumpTest, DownstreamClosedReportsAbort) {
  ASSERT_NO_FATAL_FAILURE(CreatePipes(1024));
  StartPump();

  downstream_consumer_.reset();
  WaitForCompletion();

  ASSERT_TRUE(result_);
  EXPECT_EQ(result_->net_error, net::ERR_ABORTED);
}

// With more upstream data than the downstream pipe can hold and nobody
// reading it, `Start()` fills the downstream pipe and then blocks waiting for
// room. Closing the reader at that point must abort the pump.
TEST_F(BodyPumpTest, DownstreamClosedWhileBlockedOnWrite) {
  const std::string body(4096, 'x');
  ASSERT_NO_FATAL_FAILURE(
      CreatePipes(/*upstream_capacity=*/4096, /*downstream_capacity=*/1024));
  ASSERT_EQ(upstream_producer_->WriteAllData(base::as_byte_span(body)),
            MOJO_RESULT_OK);
  StartPump();
  EXPECT_FALSE(result_);

  downstream_consumer_.reset();
  WaitForCompletion();

  ASSERT_TRUE(result_);
  EXPECT_EQ(result_->net_error, net::ERR_ABORTED);
  EXPECT_EQ(result_->body_size, 1024u);
  // The hash covers exactly the delivered prefix.
  EXPECT_EQ(result_->body_hash,
            crypto::hash::Sha256(std::string_view(body).substr(0, 1024)));
}

// If the upstream producer finishes and closes while the pump is blocked on a
// full downstream pipe, draining the downstream pipe must resume the pump and
// deliver the rest of the body cleanly.
TEST_F(BodyPumpTest, UpstreamClosedWhileBlockedOnWrite) {
  const std::string body(4096, 'z');
  ASSERT_NO_FATAL_FAILURE(
      CreatePipes(/*upstream_capacity=*/4096, /*downstream_capacity=*/1024));
  ASSERT_EQ(upstream_producer_->WriteAllData(base::as_byte_span(body)),
            MOJO_RESULT_OK);
  StartPump();
  EXPECT_FALSE(result_);

  upstream_producer_.reset();
  EXPECT_FALSE(result_);

  Drainer drainer(std::move(downstream_consumer_));
  WaitForCompletion();
  ASSERT_TRUE(base::test::RunUntil([&] { return drainer.complete(); }));

  ASSERT_TRUE(result_);
  EXPECT_EQ(result_->net_error, net::OK);
  EXPECT_EQ(result_->body_size, body.size());
  EXPECT_EQ(result_->body_hash, crypto::hash::Sha256(body));
  EXPECT_EQ(drainer.data(), body);
}

// Destroying a pump mid-transfer closes its pipes without running the
// completion callback.
TEST_F(BodyPumpTest, DestroyedMidTransferDoesNotRunCallback) {
  ASSERT_NO_FATAL_FAILURE(CreatePipes(1024));
  StartPump();

  pump_.reset();
  Drainer drainer(std::move(downstream_consumer_));
  ASSERT_TRUE(base::test::RunUntil([&] { return drainer.complete(); }));

  EXPECT_TRUE(drainer.data().empty());
  EXPECT_FALSE(result_);
}

// The completion callback is allowed to destroy the pump.
TEST_F(BodyPumpTest, CanBeDestroyedFromCompletionCallback) {
  ASSERT_NO_FATAL_FAILURE(CreatePipes(1024));
  base::RunLoop run_loop;
  pump_ = std::make_unique<BodyPump>(
      std::move(upstream_consumer_), std::move(downstream_producer_),
      base::BindLambdaForTesting([&](const BodyPump::Result& result) {
        pump_.reset();
        run_loop.Quit();
      }));
  pump_->Start();

  WriteUpstreamAndClose("abc");
  run_loop.Run();
  EXPECT_FALSE(pump_);
}

// When more than `kMaxBytesPerTurn` is immediately buffered, `Start()` yields
// after one turn's budget and finishes on subsequent tasks. The body uses a
// repeating pattern so a misaligned slice at a turn boundary would show up in
// the downstream byte comparison.
TEST_F(BodyPumpTest, YieldsAfterMaxBytesPerTurn) {
  std::string body;
  body.reserve(BodyPump::kMaxBytesPerTurn * 2);
  while (body.size() < BodyPump::kMaxBytesPerTurn * 2) {
    body += "0123456789";
  }
  ASSERT_NO_FATAL_FAILURE(
      CreatePipes(base::checked_cast<uint32_t>(body.size())));
  ASSERT_EQ(upstream_producer_->WriteAllData(base::as_byte_span(body)),
            MOJO_RESULT_OK);
  upstream_producer_.reset();

  StartPump();
  EXPECT_FALSE(result_);

  Drainer drainer(std::move(downstream_consumer_));
  WaitForCompletion();
  ASSERT_TRUE(base::test::RunUntil([&] { return drainer.complete(); }));

  ASSERT_TRUE(result_);
  EXPECT_EQ(result_->net_error, net::OK);
  EXPECT_EQ(result_->body_size, body.size());
  EXPECT_EQ(result_->body_hash, crypto::hash::Sha256(body));
  EXPECT_EQ(drainer.data(), body);
}

// After a turn that copies the whole body and parks on the readable watcher
// (upstream still open), the renderer closes its end and then the network
// service closes its end before the broker sequence runs either notification.
// The peer-closed notification runs first, and the pump must still report
// `net::OK` because every upstream byte was already committed downstream.
TEST_F(BodyPumpTest, DownstreamClosedAfterTurnBoundaryEof) {
  const std::string body(BodyPump::kMaxBytesPerTurn, 'w');
  ASSERT_NO_FATAL_FAILURE(
      CreatePipes(base::checked_cast<uint32_t>(body.size())));
  ASSERT_EQ(upstream_producer_->WriteAllData(base::as_byte_span(body)),
            MOJO_RESULT_OK);

  // Copies exactly one turn's budget and arms the readable watcher. Nothing is
  // posted yet because the upstream producer is still open.
  StartPump();
  EXPECT_FALSE(result_);

  downstream_consumer_.reset();
  upstream_producer_.reset();
  WaitForCompletion();

  ASSERT_TRUE(result_);
  EXPECT_EQ(result_->net_error, net::OK);
  EXPECT_EQ(result_->body_size, body.size());
  EXPECT_EQ(result_->body_hash, crypto::hash::Sha256(body));
}

}  // namespace

}  // namespace resource_broker
