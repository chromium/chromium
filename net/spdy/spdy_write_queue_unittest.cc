// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/spdy/spdy_write_queue.h"

#include <cstddef>
#include <cstring>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_with_source.h"
#include "net/spdy/spdy_buffer_producer.h"
#include "net/spdy/spdy_stream.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

const char kOriginal[] = "original";
const char kRequeued[] = "requeued";

class SpdyWriteQueueTest : public ::testing::Test {};

// Makes a SpdyFrameProducer producing a frame with the data in the
// given string.
std::unique_ptr<SpdyBufferProducer> StringToProducer(const std::string& s) {
  auto data = std::make_unique<char[]>(s.size());
  std::memcpy(data.get(), s.data(), s.size());
  auto frame =
      std::make_unique<spdy::SpdySerializedFrame>(std::move(data), s.size());
  auto buffer = std::make_unique<SpdyBuffer>(std::move(frame));
  return std::make_unique<SimpleBufferProducer>(std::move(buffer));
}

// Makes a SpdyBufferProducer producing a frame with the data in the
// given int (converted to a string).
std::unique_ptr<SpdyBufferProducer> IntToProducer(int i) {
  return StringToProducer(base::NumberToString(i));
}

// Producer whose produced buffer will enqueue yet another buffer into the
// SpdyWriteQueue upon destruction.
class RequeingBufferProducer : public SpdyBufferProducer {
 public:
  explicit RequeingBufferProducer(SpdyWriteQueue* queue) {
    buffer_ = std::make_unique<SpdyBuffer>(kOriginal, std::size(kOriginal));
    buffer_->AddConsumeCallback(
        base::BindRepeating(RequeingBufferProducer::ConsumeCallback, queue));
  }

  std::unique_ptr<SpdyBuffer> ProduceBuffer() override {
    return std::move(buffer_);
  }

  static void ConsumeCallback(SpdyWriteQueue* queue,
                              size_t size,
                              SpdyBuffer::ConsumeSource source) {
    auto buffer = std::make_unique<SpdyBuffer>(kRequeued, std::size(kRequeued));
    auto buffer_producer =
        std::make_unique<SimpleBufferProducer>(std::move(buffer));

    queue->Enqueue(MEDIUM, spdy::SpdyFrameType::RST_STREAM,
                   std::move(buffer_producer), base::WeakPtr<SpdyStream>(),
                   TRAFFIC_ANNOTATION_FOR_TESTS);
  }

 private:
  std::unique_ptr<SpdyBuffer> buffer_;
};

// Produces a frame with the given producer and returns a copy of its
// data as a string.
std::string ProducerToString(std::unique_ptr<SpdyBufferProducer> producer) {
  std::unique_ptr<SpdyBuffer> buffer = producer->ProduceBuffer();
  return std::string(buffer->GetRemainingData(), buffer->GetRemainingSize());
}

// Produces a frame with the given producer and returns a copy of its
// data as an int (converted from a string).
int ProducerToInt(std::unique_ptr<SpdyBufferProducer> producer) {
  int i = 0;
  EXPECT_TRUE(base::StringToInt(ProducerToString(std::move(producer)), &i));
  return i;
}

// Makes a SpdyStream with the given priority and a NULL SpdySession
// -- be careful to not call any functions that expect the session to
// be there.
std::unique_ptr<SpdyStream> MakeTestStream(RequestPriority priority) {
  return std::make_unique<SpdyStream>(
      SPDY_BIDIRECTIONAL_STREAM, base::WeakPtr<SpdySession>(), GURL(), priority,
      0, 0, NetLogWithSource(), TRAFFIC_ANNOTATION_FOR_TESTS,
      false /* detect_broken_connection */);
}

// Add some frame producers of different priority. The producers
// should be dequeued in priority order with their associated stream.
TEST_F(SpdyWriteQueueTest, DequeuesByPriority) {
  SpdyWriteQueue write_queue;

  std::unique_ptr<SpdyBufferProducer> producer_low = StringToProducer("LOW");
  std::unique_ptr<SpdyBufferProducer> producer_medium =
      StringToProducer("MEDIUM");
  std::unique_ptr<SpdyBufferProducer> producer_highest =
      StringToProducer("HIGHEST");

  std::unique_ptr<SpdyStream> stream_medium = MakeTestStream(MEDIUM);
  std::unique_ptr<SpdyStream> stream_highest = MakeTestStream(HIGHEST);

  // A NULL stream should still work.
  write_queue.Enqueue(LOW, spdy::SpdyFrameType::HEADERS,
                      std::move(producer_low), base::WeakPtr<SpdyStream>(),
                      TRAFFIC_ANNOTATION_FOR_TESTS);
  write_queue.Enqueue(MEDIUM, spdy::SpdyFrameType::HEADERS,
                      std::move(producer_medium), stream_medium->GetWeakPtr(),
                      TRAFFIC_ANNOTATION_FOR_TESTS);
  write_queue.Enqueue(HIGHEST, spdy::SpdyFrameType::RST_STREAM,
                      std::move(producer_highest), stream_highest->GetWeakPtr(),
                      TRAFFIC_ANNOTATION_FOR_TESTS);

  spdy::SpdyFrameType frame_type = spdy::SpdyFrameType::DATA;
  std::unique_ptr<SpdyBufferProducer> frame_producer;
  base::WeakPtr<SpdyStream> stream;
  MutableNetworkTrafficAnnotationTag traffic_annotation;
  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                  &traffic_annotation));
  EXPECT_EQ(spdy::SpdyFrameType::RST_STREAM, frame_type);
  EXPECT_EQ("HIGHEST", ProducerToString(std::move(frame_producer)));
  EXPECT_EQ(stream_highest.get(), stream.get());

  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                  &traffic_annotation));
  EXPECT_EQ(spdy::SpdyFrameType::HEADERS, frame_type);
  EXPECT_EQ("MEDIUM", ProducerToString(std::move(frame_producer)));
  EXPECT_EQ(stream_medium.get(), stream.get());

  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                  &traffic_annotation));
  EXPECT_EQ(spdy::SpdyFrameType::HEADERS, frame_type);
  EXPECT_EQ("LOW", ProducerToString(std::move(frame_producer)));
  EXPECT_EQ(nullptr, stream.get());

  EXPECT_FALSE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                   &traffic_annotation));
}

// Add some frame producers with the same priority. The producers
// should be dequeued in FIFO order with their associated stream.
TEST_F(SpdyWriteQueueTest, DequeuesFIFO) {
  SpdyWriteQueue write_queue;

  std::unique_ptr<SpdyBufferProducer> producer1 = IntToProducer(1);
  std::unique_ptr<SpdyBufferProducer> producer2 = IntToProducer(2);
  std::unique_ptr<SpdyBufferProducer> producer3 = IntToProducer(3);

  std::unique_ptr<SpdyStream> stream1 = MakeTestStream(DEFAULT_PRIORITY);
  std::unique_ptr<SpdyStream> stream2 = MakeTestStream(DEFAULT_PRIORITY);
  std::unique_ptr<SpdyStream> stream3 = MakeTestStream(DEFAULT_PRIORITY);

  write_queue.Enqueue(DEFAULT_PRIORITY, spdy::SpdyFrameType::HEADERS,
                      std::move(producer1), stream1->GetWeakPtr(),
                      TRAFFIC_ANNOTATION_FOR_TESTS);
  write_queue.Enqueue(DEFAULT_PRIORITY, spdy::SpdyFrameType::HEADERS,
                      std::move(producer2), stream2->GetWeakPtr(),
                      TRAFFIC_ANNOTATION_FOR_TESTS);
  write_queue.Enqueue(DEFAULT_PRIORITY, spdy::SpdyFrameType::RST_STREAM,
                      std::move(producer3), stream3->GetWeakPtr(),
                      TRAFFIC_ANNOTATION_FOR_TESTS);

  spdy::SpdyFrameType frame_type = spdy::SpdyFrameType::DATA;
  std::unique_ptr<SpdyBufferProducer> frame_producer;
  base::WeakPtr<SpdyStream> stream;
  MutableNetworkTrafficAnnotationTag traffic_annotation;
  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                  &traffic_annotation));
  EXPECT_EQ(spdy::SpdyFrameType::HEADERS, frame_type);
  EXPECT_EQ(1, ProducerToInt(std::move(frame_producer)));
  EXPECT_EQ(stream1.get(), stream.get());

  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                  &traffic_annotation));
  EXPECT_EQ(spdy::SpdyFrameType::HEADERS, frame_type);
  EXPECT_EQ(2, ProducerToInt(std::move(frame_producer)));
  EXPECT_EQ(stream2.get(), stream.get());

  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                  &traffic_annotation));
  EXPECT_EQ(spdy::SpdyFrameType::RST_STREAM, frame_type);
  EXPECT_EQ(3, ProducerToInt(std::move(frame_producer)));
  EXPECT_EQ(stream3.get(), stream.get());

  EXPECT_FALSE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                   &traffic_annotation));
}

// Enqueue a bunch of writes and then call
// RemovePendingWritesForStream() on one of the streams. No dequeued
// write should be for that stream.
TEST_F(SpdyWriteQueueTest, RemovePendingWritesForStream) {
  SpdyWriteQueue write_queue;

  std::unique_ptr<SpdyStream> stream1 = MakeTestStream(DEFAULT_PRIORITY);
  std::unique_ptr<SpdyStream> stream2 = MakeTestStream(DEFAULT_PRIORITY);

  for (int i = 0; i < 100; ++i) {
    base::WeakPtr<SpdyStream> stream =
        (((i % 3) == 0) ? stream1 : stream2)->GetWeakPtr();
    write_queue.Enqueue(DEFAULT_PRIORITY, spdy::SpdyFrameType::HEADERS,
                        IntToProducer(i), stream, TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  write_queue.RemovePendingWritesForStream(stream2.get());

  for (int i = 0; i < 100; i += 3) {
    spdy::SpdyFrameType frame_type = spdy::SpdyFrameType::DATA;
    std::unique_ptr<SpdyBufferProducer> frame_producer;
    base::WeakPtr<SpdyStream> stream;
    MutableNetworkTrafficAnnotationTag traffic_annotation;
    ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                    &traffic_annotation));
    EXPECT_EQ(spdy::SpdyFrameType::HEADERS, frame_type);
    EXPECT_EQ(i, ProducerToInt(std::move(frame_producer)));
    EXPECT_EQ(stream1.get(), stream.get());
    EXPECT_EQ(MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
              traffic_annotation);
  }

  spdy::SpdyFrameType frame_type = spdy::SpdyFrameType::DATA;
  std::unique_ptr<SpdyBufferProducer> frame_producer;
  base::WeakPtr<SpdyStream> stream;
  MutableNetworkTrafficAnnotationTag traffic_annotation;
  EXPECT_FALSE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                   &traffic_annotation));
}

// Enqueue a bunch of writes and then call
// RemovePendingWritesForStreamsAfter(). No dequeued write should be for
// those streams without a stream id, or with a stream_id after that
// argument.
TEST_F(SpdyWriteQueueTest, RemovePendingWritesForStreamsAfter) {
  SpdyWriteQueue write_queue;

  std::unique_ptr<SpdyStream> stream1 = MakeTestStream(DEFAULT_PRIORITY);
  stream1->set_stream_id(1);
  std::unique_ptr<SpdyStream> stream2 = MakeTestStream(DEFAULT_PRIORITY);
  stream2->set_stream_id(3);
  std::unique_ptr<SpdyStream> stream3 = MakeTestStream(DEFAULT_PRIORITY);
  stream3->set_stream_id(5);
  // No stream id assigned.
  std::unique_ptr<SpdyStream> stream4 = MakeTestStream(DEFAULT_PRIORITY);
  base::WeakPtr<SpdyStream> streams[] = {
    stream1->GetWeakPtr(), stream2->GetWeakPtr(),
    stream3->GetWeakPtr(), stream4->GetWeakPtr()
  };

  for (int i = 0; i < 100; ++i) {
    write_queue.Enqueue(DEFAULT_PRIORITY, spdy::SpdyFrameType::HEADERS,
                        IntToProducer(i), streams[i % std::size(streams)],
                        TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  write_queue.RemovePendingWritesForStreamsAfter(stream1->stream_id());

  for (int i = 0; i < 100; i += std::size(streams)) {
    spdy::SpdyFrameType frame_type = spdy::SpdyFrameType::DATA;
    std::unique_ptr<SpdyBufferProducer> frame_producer;
    base::WeakPtr<SpdyStream> stream;
    MutableNetworkTrafficAnnotationTag traffic_annotation;
    ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                    &traffic_annotation))
        << "Unable to Dequeue i: " << i;
    EXPECT_EQ(spdy::SpdyFrameType::HEADERS, frame_type);
    EXPECT_EQ(i, ProducerToInt(std::move(frame_producer)));
    EXPECT_EQ(stream1.get(), stream.get());
    EXPECT_EQ(MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
              traffic_annotation);
  }

  spdy::SpdyFrameType frame_type = spdy::SpdyFrameType::DATA;
  std::unique_ptr<SpdyBufferProducer> frame_producer;
  base::WeakPtr<SpdyStream> stream;
  MutableNetworkTrafficAnnotationTag traffic_annotation;
  EXPECT_FALSE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                   &traffic_annotation));
}

// Enqueue a bunch of writes and then call Clear(). The write queue
// should clean up the memory properly, and Dequeue() should return
// false.
TEST_F(SpdyWriteQueueTest, Clear) {
  SpdyWriteQueue write_queue;

  for (int i = 0; i < 100; ++i) {
    write_queue.Enqueue(DEFAULT_PRIORITY, spdy::SpdyFrameType::HEADERS,
                        IntToProducer(i), base::WeakPtr<SpdyStream>(),
                        TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  write_queue.Clear();

  spdy::SpdyFrameType frame_type = spdy::SpdyFrameType::DATA;
  std::unique_ptr<SpdyBufferProducer> frame_producer;
  base::WeakPtr<SpdyStream> stream;
  MutableNetworkTrafficAnnotationTag traffic_annotation;
  EXPECT_FALSE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                   &traffic_annotation));
}

TEST_F(SpdyWriteQueueTest, RequeingProducerWithoutReentrance) {
  SpdyWriteQueue queue;
  queue.Enqueue(DEFAULT_PRIORITY, spdy::SpdyFrameType::HEADERS,
                std::make_unique<RequeingBufferProducer>(&queue),
                base::WeakPtr<SpdyStream>(), TRAFFIC_ANNOTATION_FOR_TESTS);
  {
    spdy::SpdyFrameType frame_type;
    std::unique_ptr<SpdyBufferProducer> producer;
    base::WeakPtr<SpdyStream> stream;
    MutableNetworkTrafficAnnotationTag traffic_annotation;

    EXPECT_TRUE(
        queue.Dequeue(&frame_type, &producer, &stream, &traffic_annotation));
    EXPECT_TRUE(queue.IsEmpty());
    EXPECT_EQ(std::string(kOriginal),
              producer->ProduceBuffer()->GetRemainingData());
  }
  // |producer| was destroyed, and a buffer is re-queued.
  EXPECT_FALSE(queue.IsEmpty());

  spdy::SpdyFrameType frame_type;
  std::unique_ptr<SpdyBufferProducer> producer;
  base::WeakPtr<SpdyStream> stream;
  MutableNetworkTrafficAnnotationTag traffic_annotation;

  EXPECT_TRUE(
      queue.Dequeue(&frame_type, &producer, &stream, &traffic_annotation));
  EXPECT_EQ(std::string(kRequeued),
            producer->ProduceBuffer()->GetRemainingData());
}

TEST_F(SpdyWriteQueueTest, ReentranceOnClear) {
  SpdyWriteQueue queue;
  queue.Enqueue(DEFAULT_PRIORITY, spdy::SpdyFrameType::HEADERS,
                std::make_unique<RequeingBufferProducer>(&queue),
                base::WeakPtr<SpdyStream>(), TRAFFIC_ANNOTATION_FOR_TESTS);

  queue.Clear();
  EXPECT_FALSE(queue.IsEmpty());

  spdy::SpdyFrameType frame_type;
  std::unique_ptr<SpdyBufferProducer> producer;
  base::WeakPtr<SpdyStream> stream;
  MutableNetworkTrafficAnnotationTag traffic_annotation;

  EXPECT_TRUE(
      queue.Dequeue(&frame_type, &producer, &stream, &traffic_annotation));
  EXPECT_EQ(std::string(kRequeued),
            producer->ProduceBuffer()->GetRemainingData());
}

TEST_F(SpdyWriteQueueTest, ReentranceOnRemovePendingWritesAfter) {
  std::unique_ptr<SpdyStream> stream = MakeTestStream(DEFAULT_PRIORITY);
  stream->set_stream_id(2);

  SpdyWriteQueue queue;
  queue.Enqueue(DEFAULT_PRIORITY, spdy::SpdyFrameType::HEADERS,
                std::make_unique<RequeingBufferProducer>(&queue),
                stream->GetWeakPtr(), TRAFFIC_ANNOTATION_FOR_TESTS);

  queue.RemovePendingWritesForStreamsAfter(1);
  EXPECT_FALSE(queue.IsEmpty());

  spdy::SpdyFrameType frame_type;
  std::unique_ptr<SpdyBufferProducer> producer;
  base::WeakPtr<SpdyStream> weak_stream;
  MutableNetworkTrafficAnnotationTag traffic_annotation;

  EXPECT_TRUE(
      queue.Dequeue(&frame_type, &producer, &weak_stream, &traffic_annotation));
  EXPECT_EQ(std::string(kRequeued),
            producer->ProduceBuffer()->GetRemainingData());
}

TEST_F(SpdyWriteQueueTest, ReentranceOnRemovePendingWritesForStream) {
  std::unique_ptr<SpdyStream> stream = MakeTestStream(DEFAULT_PRIORITY);
  stream->set_stream_id(2);

  SpdyWriteQueue queue;
  queue.Enqueue(DEFAULT_PRIORITY, spdy::SpdyFrameType::HEADERS,
                std::make_unique<RequeingBufferProducer>(&queue),
                stream->GetWeakPtr(), TRAFFIC_ANNOTATION_FOR_TESTS);

  queue.RemovePendingWritesForStream(stream.get());
  EXPECT_FALSE(queue.IsEmpty());

  spdy::SpdyFrameType frame_type;
  std::unique_ptr<SpdyBufferProducer> producer;
  base::WeakPtr<SpdyStream> weak_stream;
  MutableNetworkTrafficAnnotationTag traffic_annotation;

  EXPECT_TRUE(
      queue.Dequeue(&frame_type, &producer, &weak_stream, &traffic_annotation));
  EXPECT_EQ(std::string(kRequeued),
            producer->ProduceBuffer()->GetRemainingData());
}

TEST_F(SpdyWriteQueueTest, ChangePriority) {
  SpdyWriteQueue write_queue;

  std::unique_ptr<SpdyBufferProducer> producer1 = IntToProducer(1);
  std::unique_ptr<SpdyBufferProducer> producer2 = IntToProducer(2);
  std::unique_ptr<SpdyBufferProducer> producer3 = IntToProducer(3);

  std::unique_ptr<SpdyStream> stream1 = MakeTestStream(HIGHEST);
  std::unique_ptr<SpdyStream> stream2 = MakeTestStream(MEDIUM);
  std::unique_ptr<SpdyStream> stream3 = MakeTestStream(LOW);

  write_queue.Enqueue(HIGHEST, spdy::SpdyFrameType::HEADERS,
                      std::move(producer1), stream1->GetWeakPtr(),
                      TRAFFIC_ANNOTATION_FOR_TESTS);
  write_queue.Enqueue(MEDIUM, spdy::SpdyFrameType::DATA, std::move(producer2),
                      stream2->GetWeakPtr(), TRAFFIC_ANNOTATION_FOR_TESTS);
  write_queue.Enqueue(LOW, spdy::SpdyFrameType::RST_STREAM,
                      std::move(producer3), stream3->GetWeakPtr(),
                      TRAFFIC_ANNOTATION_FOR_TESTS);

  write_queue.ChangePriorityOfWritesForStream(stream3.get(), LOW, HIGHEST);

  spdy::SpdyFrameType frame_type = spdy::SpdyFrameType::DATA;
  std::unique_ptr<SpdyBufferProducer> frame_producer;
  base::WeakPtr<SpdyStream> stream;
  MutableNetworkTrafficAnnotationTag traffic_annotation;
  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                  &traffic_annotation));
  EXPECT_EQ(spdy::SpdyFrameType::HEADERS, frame_type);
  EXPECT_EQ(1, ProducerToInt(std::move(frame_producer)));
  EXPECT_EQ(stream1.get(), stream.get());

  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                  &traffic_annotation));
  EXPECT_EQ(spdy::SpdyFrameType::RST_STREAM, frame_type);
  EXPECT_EQ(3, ProducerToInt(std::move(frame_producer)));
  EXPECT_EQ(stream3.get(), stream.get());

  ASSERT_TRUE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                  &traffic_annotation));
  EXPECT_EQ(spdy::SpdyFrameType::DATA, frame_type);
  EXPECT_EQ(2, ProducerToInt(std::move(frame_producer)));
  EXPECT_EQ(stream2.get(), stream.get());

  EXPECT_FALSE(write_queue.Dequeue(&frame_type, &frame_producer, &stream,
                                   &traffic_annotation));
}

}  // namespace

}  // namespace net
