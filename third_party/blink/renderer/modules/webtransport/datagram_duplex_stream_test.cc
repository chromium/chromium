// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/datagram_duplex_stream.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/web_transport.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/webtransport/test_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

constexpr int32_t kInitialOutgoingHighWaterMark = 1;

// Tiny implementation of network::mojom::blink::WebTransport with only the
// functionality needed for these tests.
class StubWebTransport final : public network::mojom::blink::WebTransport {
 public:
  explicit StubWebTransport(
      mojo::PendingReceiver<network::mojom::blink::WebTransport>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  std::optional<base::TimeDelta> OutgoingDatagramExpirationDurationValue() {
    return outgoing_datagram_expiration_duration_value_;
  }

  // Implementation of WebTransport.
  void SendDatagram(base::span<const uint8_t> data,
                    base::OnceCallback<void(bool)>) override {
    NOTREACHED_IN_MIGRATION();
  }

  void CreateStream(
      mojo::ScopedDataPipeConsumerHandle output_consumer,
      mojo::ScopedDataPipeProducerHandle input_producer,
      base::OnceCallback<void(bool, uint32_t)> callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void AcceptBidirectionalStream(
      base::OnceCallback<void(uint32_t,
                              mojo::ScopedDataPipeConsumerHandle,
                              mojo::ScopedDataPipeProducerHandle)> callback)
      override {
    DCHECK(!ignored_accept_callback_);
    // This method is always called. We have to retain the callback to avoid an
    // error about early destruction, but never call it.
    ignored_accept_callback_ = std::move(callback);
  }

  void AcceptUnidirectionalStream(
      base::OnceCallback<void(uint32_t, mojo::ScopedDataPipeConsumerHandle)>
          callback) override {
    DCHECK(!ignored_unidirectional_stream_callback_);
    // This method is always called. We have to retain the callback to avoid an
    // error about early destruction, but never call it.
    ignored_unidirectional_stream_callback_ = std::move(callback);
  }

  void SendFin(uint32_t stream_id) override {}

  void AbortStream(uint32_t stream_id, uint8_t code) override {}

  void StopSending(uint32_t stream_id, uint8_t code) override {}

  void SetOutgoingDatagramExpirationDuration(base::TimeDelta value) override {
    outgoing_datagram_expiration_duration_value_ = value;
  }

  void GetStats(GetStatsCallback callback) override {
    std::move(callback).Run(nullptr);
  }

  void Close(network::mojom::blink::WebTransportCloseInfoPtr) override {}

 private:
  base::OnceCallback<void(uint32_t,
                          mojo::ScopedDataPipeConsumerHandle,
                          mojo::ScopedDataPipeProducerHandle)>
      ignored_accept_callback_;
  base::OnceCallback<void(uint32_t, mojo::ScopedDataPipeConsumerHandle)>
      ignored_unidirectional_stream_callback_;
  mojo::Receiver<network::mojom::blink::WebTransport> receiver_;
  std::optional<base::TimeDelta> outgoing_datagram_expiration_duration_value_;
};

// This class sets up a connected blink::WebTransport object using a
// StubWebTransport and provides access to both.
class ScopedWebTransport final {
  STACK_ALLOCATED();

 public:
  // This constructor runs the event loop.
  explicit ScopedWebTransport(const V8TestingScope& scope) {
    creator_.Init(scope.GetScriptState(),
                  WTF::BindRepeating(&ScopedWebTransport::CreateStub,
                                     weak_ptr_factory_.GetWeakPtr()));
  }

  WebTransport* GetWebTransport() const { return creator_.GetWebTransport(); }
  StubWebTransport* Stub() const { return stub_.get(); }

 private:
  void CreateStub(mojo::PendingRemote<network::mojom::blink::WebTransport>&
                      web_transport_to_pass) {
    stub_ = std::make_unique<StubWebTransport>(
        web_transport_to_pass.InitWithNewPipeAndPassReceiver());
  }

  TestWebTransportCreator creator_;
  std::unique_ptr<StubWebTransport> stub_;

  base::WeakPtrFactory<ScopedWebTransport> weak_ptr_factory_{this};
};

class ScopedDatagramDuplexStream final {
  STACK_ALLOCATED();

 public:
  ScopedDatagramDuplexStream()
      : scoped_web_transport_(v8_testing_scope_),
        duplex_(MakeGarbageCollected<DatagramDuplexStream>(
            scoped_web_transport_.GetWebTransport(),
            kInitialOutgoingHighWaterMark)) {}
  ScopedDatagramDuplexStream(const ScopedDatagramDuplexStream&) = delete;
  ScopedDatagramDuplexStream& operator=(const ScopedDatagramDuplexStream&) =
      delete;

  DatagramDuplexStream* Duplex() { return duplex_; }

  StubWebTransport* Stub() { return scoped_web_transport_.Stub(); }

 private:
  V8TestingScope v8_testing_scope_;
  ScopedWebTransport scoped_web_transport_;
  DatagramDuplexStream* const duplex_;
};

TEST(DatagramDuplexStreamTest, Defaults) {
  test::TaskEnvironment task_environment;
  ScopedDatagramDuplexStream scope;
  auto* duplex = scope.Duplex();
  EXPECT_FALSE(duplex->incomingMaxAge().has_value());
  EXPECT_FALSE(duplex->outgoingMaxAge().has_value());
  EXPECT_EQ(duplex->incomingHighWaterMark(), kDefaultIncomingHighWaterMark);
  EXPECT_EQ(duplex->outgoingHighWaterMark(), kInitialOutgoingHighWaterMark);
}

TEST(DatagramDuplexStreamTest, SetIncomingMaxAge) {
  test::TaskEnvironment task_environment;
  ScopedDatagramDuplexStream scope;
  auto* duplex = scope.Duplex();

  duplex->setIncomingMaxAge(1.0);
  ASSERT_TRUE(duplex->incomingMaxAge().has_value());
  EXPECT_EQ(duplex->incomingMaxAge().value(), 1.0);

  duplex->setIncomingMaxAge(std::nullopt);
  ASSERT_FALSE(duplex->incomingMaxAge().has_value());

  duplex->setIncomingMaxAge(0.0);
  ASSERT_FALSE(duplex->incomingMaxAge().has_value());

  duplex->setIncomingMaxAge(-1.0);
  ASSERT_FALSE(duplex->incomingMaxAge().has_value());
}

TEST(DatagramDuplexStreamTest, SetOutgoingMaxAge) {
  test::TaskEnvironment task_environment;
  ScopedDatagramDuplexStream scope;
  auto* duplex = scope.Duplex();
  auto* stub = scope.Stub();

  duplex->setOutgoingMaxAge(1.0);
  ASSERT_TRUE(duplex->outgoingMaxAge().has_value());
  EXPECT_EQ(duplex->outgoingMaxAge().value(), 1.0);
  test::RunPendingTasks();
  auto expiration_duration = stub->OutgoingDatagramExpirationDurationValue();
  ASSERT_TRUE(expiration_duration.has_value());
  EXPECT_EQ(expiration_duration.value(), base::Milliseconds(1.0));

  duplex->setOutgoingMaxAge(std::nullopt);
  ASSERT_FALSE(duplex->outgoingMaxAge().has_value());
  test::RunPendingTasks();
  expiration_duration = stub->OutgoingDatagramExpirationDurationValue();
  ASSERT_TRUE(expiration_duration.has_value());
  EXPECT_EQ(expiration_duration.value(), base::Milliseconds(0.0));

  duplex->setOutgoingMaxAge(0.5);
  ASSERT_TRUE(duplex->outgoingMaxAge().has_value());
  EXPECT_EQ(duplex->outgoingMaxAge().value(), 0.5);
  test::RunPendingTasks();
  expiration_duration = stub->OutgoingDatagramExpirationDurationValue();
  ASSERT_TRUE(expiration_duration.has_value());
  EXPECT_EQ(expiration_duration.value(), base::Milliseconds(0.5));

  duplex->setOutgoingMaxAge(0.0);
  ASSERT_TRUE(duplex->outgoingMaxAge().has_value());
  EXPECT_EQ(duplex->outgoingMaxAge().value(), 0.5);  // unchanged
  test::RunPendingTasks();
  expiration_duration = stub->OutgoingDatagramExpirationDurationValue();
  ASSERT_TRUE(expiration_duration.has_value());
  EXPECT_EQ(expiration_duration.value(),
            base::Milliseconds(0.5));  // Unchanged

  duplex->setOutgoingMaxAge(-1.0);
  ASSERT_TRUE(duplex->outgoingMaxAge().has_value());
  EXPECT_EQ(duplex->outgoingMaxAge().value(), 0.5);  // unchanged
  test::RunPendingTasks();
  expiration_duration = stub->OutgoingDatagramExpirationDurationValue();
  ASSERT_TRUE(expiration_duration.has_value());
  EXPECT_EQ(expiration_duration.value(),
            base::Milliseconds(0.5));  // Unchanged
}

TEST(DatagramDuplexStreamTest, SetIncomingHighWaterMark) {
  test::TaskEnvironment task_environment;
  ScopedDatagramDuplexStream scope;
  auto* duplex = scope.Duplex();

  duplex->setIncomingHighWaterMark(10);
  EXPECT_EQ(duplex->incomingHighWaterMark(), 10);

  duplex->setIncomingHighWaterMark(0);
  EXPECT_EQ(duplex->incomingHighWaterMark(), 0);

  duplex->setIncomingHighWaterMark(-1);
  EXPECT_EQ(duplex->incomingHighWaterMark(), 0);
}

TEST(DatagramDuplexStreamTest, SetOutgoingHighWaterMark) {
  test::TaskEnvironment task_environment;
  ScopedDatagramDuplexStream scope;
  auto* duplex = scope.Duplex();

  duplex->setOutgoingHighWaterMark(10);
  EXPECT_EQ(duplex->outgoingHighWaterMark(), 10);

  duplex->setOutgoingHighWaterMark(0);
  EXPECT_EQ(duplex->outgoingHighWaterMark(), 0);

  duplex->setOutgoingHighWaterMark(-1);
  EXPECT_EQ(duplex->outgoingHighWaterMark(), 0);
}

TEST(DatagramDuplexStreamTest, InitialMaxDatagramSize) {
  test::TaskEnvironment task_environment;
  ScopedDatagramDuplexStream scope;
  auto* duplex = scope.Duplex();

  EXPECT_EQ(duplex->maxDatagramSize(), 1024u);
}

}  // namespace

}  // namespace blink
