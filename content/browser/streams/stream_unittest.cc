// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_read_observer.h"
#include "content/browser/streams/stream_register_observer.h"
#include "content/browser/streams/stream_registry.h"
#include "content/browser/streams/stream_write_observer.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class StreamTest : public testing::Test {
 public:
  StreamTest() : producing_seed_key_(0) {}

  void SetUp() override { registry_.reset(new StreamRegistry()); }

  // Create a new IO buffer of the given |buffer_size| and fill it with random
  // data.
  scoped_refptr<net::IOBuffer> NewIOBuffer(size_t buffer_size) {
    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(buffer_size);
    char *bufferp = buffer->data();
    for (size_t i = 0; i < buffer_size; i++)
      bufferp[i] = (i + producing_seed_key_) % (1 << sizeof(char));
    ++producing_seed_key_;
    return buffer;
  }

 protected:
  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<StreamRegistry> registry_;

 private:
  int producing_seed_key_;
};

class TestStreamReader : public StreamReadObserver {
 public:
  TestStreamReader() : buffer_(base::MakeRefCounted<net::GrowableIOBuffer>()) {}
  ~TestStreamReader() override {}

  void Read(Stream* stream) {
    const size_t kBufferSize = 32768;
    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(kBufferSize);

    int bytes_read = 0;
    while (true) {
      Stream::StreamState state =
          stream->ReadRawData(buffer.get(), kBufferSize, &bytes_read);
      switch (state) {
        case Stream::STREAM_HAS_DATA:
          // TODO(tyoshino): Move these expectations to the beginning of Read()
          // method once Stream::Finalize() is fixed.
          EXPECT_FALSE(completed_);
          break;
        case Stream::STREAM_COMPLETE:
          completed_ = true;
          status_ = stream->GetStatus();
          return;
        case Stream::STREAM_EMPTY:
          EXPECT_FALSE(completed_);
          return;
        case Stream::STREAM_ABORTED:
          aborted_ = true;
          EXPECT_FALSE(completed_);
          return;
      }
      size_t old_capacity = buffer_->capacity();
      buffer_->SetCapacity(old_capacity + bytes_read);
      memcpy(buffer_->StartOfBuffer() + old_capacity,
             buffer->data(), bytes_read);
    }
  }

  void OnDataAvailable(Stream* stream) override { Read(stream); }

  scoped_refptr<net::GrowableIOBuffer> buffer() { return buffer_; }

  bool completed() const { return completed_; }
  bool aborted() const { return aborted_; }
  int status() const { return status_; }

 private:
  scoped_refptr<net::GrowableIOBuffer> buffer_;
  bool completed_ = false;
  bool aborted_ = false;
  int status_ = 0;
};

class TestStreamWriter : public StreamWriteObserver {
 public:
  TestStreamWriter() {}
  ~TestStreamWriter() override {}

  void Write(Stream* stream,
             scoped_refptr<net::IOBuffer> buffer,
             size_t buffer_size) {
    stream->AddData(buffer, buffer_size);
  }

  void OnSpaceAvailable(Stream* stream) override {}

  void OnClose(Stream* stream) override {}
};

class TestStreamObserver : public StreamRegisterObserver {
 public:
  TestStreamObserver(const GURL& url, StreamRegistry* registry)
      : url_(url), registry_(registry), registered_(false), stream_(nullptr) {
    registry->SetRegisterObserver(url, this);
  }
  ~TestStreamObserver() override { registry_->RemoveRegisterObserver(url_); }
  void OnStreamRegistered(Stream* stream) override {
    registered_ = true;
    stream_ = stream;
  }
  bool registered() const { return registered_; }
  Stream* stream() const { return stream_; }

 private:
  const GURL url_;
  StreamRegistry* registry_;
  bool registered_;
  Stream* stream_;
};

TEST_F(StreamTest, SetAndRemoveRegisterObserver) {
  TestStreamWriter writer1;
  TestStreamWriter writer2;
  GURL url1("blob://stream1");
  GURL url2("blob://stream2");
  std::unique_ptr<TestStreamObserver> observer1(
      new TestStreamObserver(url1, registry_.get()));
  std::unique_ptr<TestStreamObserver> observer2(
      new TestStreamObserver(url2, registry_.get()));
  scoped_refptr<Stream> stream1(new Stream(registry_.get(), &writer1, url1));
  EXPECT_TRUE(observer1->registered());
  EXPECT_EQ(observer1->stream(), stream1.get());
  EXPECT_FALSE(observer2->registered());

  observer2.reset();
  scoped_refptr<Stream> stream2(new Stream(registry_.get(), &writer2, url2));
}

TEST_F(StreamTest, SetReadObserver) {
  TestStreamReader reader;
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), &writer, url));
  EXPECT_TRUE(stream->SetReadObserver(&reader));
}

TEST_F(StreamTest, SetReadObserver_SecondFails) {
  TestStreamReader reader1;
  TestStreamReader reader2;
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), &writer, url));
  EXPECT_TRUE(stream->SetReadObserver(&reader1));
  EXPECT_FALSE(stream->SetReadObserver(&reader2));
}

TEST_F(StreamTest, SetReadObserver_TwoReaders) {
  TestStreamReader reader1;
  TestStreamReader reader2;
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), &writer, url));
  EXPECT_TRUE(stream->SetReadObserver(&reader1));

  // Once the first read observer is removed, a new one can be added.
  stream->RemoveReadObserver(&reader1);
  EXPECT_TRUE(stream->SetReadObserver(&reader2));
}

TEST_F(StreamTest, Stream) {
  TestStreamReader reader;
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), &writer, url));
  EXPECT_TRUE(stream->SetReadObserver(&reader));

  const int kBufferSize = 1000000;
  scoped_refptr<net::IOBuffer> buffer(NewIOBuffer(kBufferSize));
  writer.Write(stream.get(), buffer, kBufferSize);
  stream->Finalize(net::OK);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(reader.completed());
  EXPECT_EQ(net::OK, reader.status());

  ASSERT_EQ(reader.buffer()->capacity(), kBufferSize);
  for (int i = 0; i < kBufferSize; i++)
    EXPECT_EQ(buffer->data()[i], reader.buffer()->data()[i]);
}

TEST_F(StreamTest, Abort) {
  TestStreamReader reader;
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(new Stream(registry_.get(), &writer, url));
  EXPECT_TRUE(stream->SetReadObserver(&reader));

  stream->Abort();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(reader.completed());
  EXPECT_TRUE(reader.aborted());
}

TEST_F(StreamTest, Error) {
  TestStreamReader reader;
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(new Stream(registry_.get(), &writer, url));
  EXPECT_TRUE(stream->SetReadObserver(&reader));

  stream->Finalize(net::ERR_ACCESS_DENIED);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(reader.completed());
  EXPECT_EQ(net::ERR_ACCESS_DENIED, reader.status());
}

// Test that even if a reader receives an empty buffer, once TransferData()
// method is called on it with |source_complete| = true, following Read() calls
// on it never returns STREAM_EMPTY. Together with StreamTest.Stream above, this
// guarantees that Reader::Read() call returns only STREAM_HAS_DATA
// or STREAM_COMPLETE in |data_available_callback_| call corresponding to
// Writer::Close().
TEST_F(StreamTest, ClosedReaderDoesNotReturnStreamEmpty) {
  TestStreamReader reader;
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(
      new Stream(registry_.get(), &writer, url));
  EXPECT_TRUE(stream->SetReadObserver(&reader));

  const int kBufferSize = 0;
  scoped_refptr<net::IOBuffer> buffer(NewIOBuffer(kBufferSize));
  stream->AddData(buffer, kBufferSize);
  stream->Finalize(net::OK);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(reader.completed());
  EXPECT_EQ(0, reader.buffer()->capacity());
  EXPECT_EQ(net::OK, reader.status());
}

TEST_F(StreamTest, GetStream) {
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer, url));

  scoped_refptr<Stream> stream2 = registry_->GetStream(url);
  ASSERT_EQ(stream1, stream2);
}

TEST_F(StreamTest, GetStream_Missing) {
  TestStreamWriter writer;

  GURL url1("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer, url1));

  GURL url2("blob://stream2");
  scoped_refptr<Stream> stream2 = registry_->GetStream(url2);
  ASSERT_FALSE(stream2.get());
}

TEST_F(StreamTest, CloneStream) {
  TestStreamWriter writer;

  GURL url1("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer, url1));

  GURL url2("blob://stream2");
  ASSERT_TRUE(registry_->CloneStream(url2, url1));
  scoped_refptr<Stream> stream2 = registry_->GetStream(url2);
  ASSERT_EQ(stream1, stream2);
}

TEST_F(StreamTest, CloneStream_Missing) {
  TestStreamWriter writer;

  GURL url1("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer, url1));

  GURL url2("blob://stream2");
  GURL url3("blob://stream3");
  ASSERT_FALSE(registry_->CloneStream(url2, url3));
  scoped_refptr<Stream> stream2 = registry_->GetStream(url2);
  ASSERT_FALSE(stream2.get());
}

TEST_F(StreamTest, UnregisterStream) {
  TestStreamWriter writer;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer, url));

  registry_->UnregisterStream(url);
  scoped_refptr<Stream> stream2 = registry_->GetStream(url);
  ASSERT_FALSE(stream2.get());
}

TEST_F(StreamTest, MemoryExceedMemoryUsageLimit) {
  TestStreamWriter writer1;
  TestStreamWriter writer2;

  GURL url1("blob://stream");
  scoped_refptr<Stream> stream1(
      new Stream(registry_.get(), &writer1, url1));

  GURL url2("blob://stream2");
  scoped_refptr<Stream> stream2(
      new Stream(registry_.get(), &writer2, url2));

  const int kMaxMemoryUsage = 1500000;
  registry_->set_max_memory_usage_for_testing(kMaxMemoryUsage);

  const int kBufferSize = 1000000;
  scoped_refptr<net::IOBuffer> buffer(NewIOBuffer(kBufferSize));
  writer1.Write(stream1.get(), buffer, kBufferSize);
  // Make transfer happen.
  base::RunLoop().RunUntilIdle();

  writer2.Write(stream2.get(), buffer, kBufferSize);

  // Written data (1000000 * 2) exceeded limit (1500000). |stream2| should be
  // unregistered with |registry_|.
  EXPECT_EQ(nullptr, registry_->GetStream(url2).get());

  writer1.Write(stream1.get(), buffer, kMaxMemoryUsage - kBufferSize);
  // Should be accepted since stream2 is unregistered and the new data is not
  // so big to exceed the limit.
  EXPECT_FALSE(registry_->GetStream(url1).get() == nullptr);
}

TEST_F(StreamTest, UnderMemoryUsageLimit) {
  TestStreamWriter writer;
  TestStreamReader reader;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(new Stream(registry_.get(), &writer, url));
  EXPECT_TRUE(stream->SetReadObserver(&reader));

  registry_->set_max_memory_usage_for_testing(1500000);

  const int kBufferSize = 1000000;
  scoped_refptr<net::IOBuffer> buffer(NewIOBuffer(kBufferSize));
  writer.Write(stream.get(), buffer, kBufferSize);

  // Run loop to make |reader| consume the data.
  base::RunLoop().RunUntilIdle();

  writer.Write(stream.get(), buffer, kBufferSize);

  EXPECT_EQ(stream.get(), registry_->GetStream(url).get());
}

TEST_F(StreamTest, Flush) {
  TestStreamWriter writer;
  TestStreamReader reader;

  GURL url("blob://stream");
  scoped_refptr<Stream> stream(new Stream(registry_.get(), &writer, url));
  EXPECT_TRUE(stream->SetReadObserver(&reader));

  // If the written data size is smaller than ByteStreamWriter's (total size /
  // kFractionBufferBeforeSending), StreamReadObserver::OnDataAvailable is not
  // called.
  const int kBufferSize = 1;
  scoped_refptr<net::IOBuffer> buffer(NewIOBuffer(kBufferSize));
  writer.Write(stream.get(), buffer, kBufferSize);

  // Run loop to make |reader| consume the data.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, reader.buffer()->capacity());

  stream->Flush();

  // Run loop to make |reader| consume the data.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kBufferSize, reader.buffer()->capacity());

  EXPECT_EQ(stream.get(), registry_->GetStream(url).get());
}

TEST_F(StreamTest, AbortPendingStream) {
  TestStreamWriter writer;

  GURL url("blob://stream");
  registry_->AbortPendingStream(url);
  scoped_refptr<Stream> stream1(new Stream(registry_.get(), &writer, url));
  ASSERT_EQ(nullptr, registry_->GetStream(url).get());
}

}  // namespace content
