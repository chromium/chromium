// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_data_source_provider_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "media/base/mock_media_log.h"
#include "media/filters/hls_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using base::test::RunOnceCallback;
using testing::_;
using testing::AtLeast;
using testing::ByMove;
using testing::DoAll;
using testing::Eq;
using testing::Invoke;
using testing::NiceMock;
using testing::NotNull;
using testing::Ref;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;
using testing::StrictMock;

class HlsDataSourceProviderImplUnittest : public testing::Test {
 public:
  ~HlsDataSourceProviderImplUnittest() override = default;
  HlsDataSourceProviderImplUnittest() { RecreateImpl(); }

  void RecreateImpl() {
    auto factory = std::make_unique<MockDataSourceFactory>();
    factory_ = factory.get();
    impl_ = std::make_unique<HlsDataSourceProviderImpl>(std::move(factory));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<HlsDataSourceProviderImpl> impl_;

  raw_ptr<MockDataSourceFactory> factory_;
};

TEST_F(HlsDataSourceProviderImplUnittest, TestReadFromUrlOnce) {
  // The entire read is satisfied, so there is more to read.
  factory_->AddReadExpectation(0, 16384, 16384);
  impl_->ReadFromUrl(
      {GURL("example.com"), std::nullopt},
      base::BindOnce([](HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_EQ(stream->read_position(), 16384lu);
        ASSERT_EQ(stream->buffer_size(), 16384lu);
        ASSERT_EQ(stream->max_read_position(), std::nullopt);
        ASSERT_TRUE(stream->CanReadMore());
      }));
  task_environment_.RunUntilIdle();

  // Only got 400 bytes of requested, so the stream is _probably_ ended, but
  // we'd have to read again (and get a 0) to be sure.
  factory_->AddReadExpectation(0, 16384, 400);
  impl_->ReadFromUrl(
      {GURL("example.com"), std::nullopt},
      base::BindOnce([](HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_TRUE(stream->CanReadMore());
        ASSERT_EQ(stream->read_position(), 400lu);
        ASSERT_EQ(stream->buffer_size(), 16384lu);
        ASSERT_EQ(stream->max_read_position(), std::nullopt);
      }));
  task_environment_.RunUntilIdle();

  // The data source should only be limited to 4242 total bytes and should start
  // at an offset of 99. The read should be from 99, size of 4242.
  factory_->AddReadExpectation(99, 4242, 4242);
  impl_->ReadFromUrl(
      {GURL("example.com"), hls::types::ByteRange::Validate(4242, 99)},
      base::BindOnce([](HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_EQ(stream->read_position(), 4341lu);
        ASSERT_EQ(stream->buffer_size(), 4242lu);
        ASSERT_EQ(stream->max_read_position().value_or(0), 4341lu);
        ASSERT_FALSE(stream->CanReadMore());
      }));
  task_environment_.RunUntilIdle();
}

TEST_F(HlsDataSourceProviderImplUnittest, TestReadFromUrlThenReadAgain) {
  factory_->AddReadExpectation(0, 16384, 16384);
  factory_->AddReadExpectation(16384, 16384, 16384);
  factory_->AddReadExpectation(32768, 16384, 3);
  factory_->AddReadExpectation(32771, 16384, 0);
  impl_->ReadFromUrl(
      {GURL("example.com"), std::nullopt},
      base::BindOnce(
          [](HlsDataSourceProviderImpl* impl_ptr,
             HlsDataSourceProvider::ReadResult result) {
            ASSERT_TRUE(result.has_value());
            auto stream = std::move(result).value();
            ASSERT_EQ(stream->read_position(), 16384lu);
            ASSERT_EQ(stream->buffer_size(), 16384lu);
            ASSERT_TRUE(stream->CanReadMore());

            impl_ptr->ReadFromExistingStream(
                std::move(stream),
                base::BindOnce(
                    [](HlsDataSourceProviderImpl* impl_ptr,
                       HlsDataSourceProvider::ReadResult result) {
                      ASSERT_TRUE(result.has_value());
                      auto stream = std::move(result).value();
                      ASSERT_EQ(stream->read_position(), 32768lu);
                      ASSERT_EQ(stream->buffer_size(), 32768lu);
                      ASSERT_TRUE(stream->CanReadMore());

                      impl_ptr->ReadFromExistingStream(
                          std::move(stream),
                          base::BindOnce(
                              [](HlsDataSourceProviderImpl* impl_ptr,
                                 HlsDataSourceProvider::ReadResult result) {
                                ASSERT_TRUE(result.has_value());
                                auto stream = std::move(result).value();
                                ASSERT_EQ(stream->read_position(), 32771lu);
                                ASSERT_EQ(stream->buffer_size(), 49152lu);
                                ASSERT_TRUE(stream->CanReadMore());

                                impl_ptr->ReadFromExistingStream(
                                    std::move(stream),
                                    base::BindOnce(
                                        [](HlsDataSourceProvider::ReadResult
                                               result) {
                                          ASSERT_TRUE(result.has_value());
                                          auto stream =
                                              std::move(result).value();
                                          ASSERT_EQ(stream->read_position(),
                                                    32771lu);
                                          ASSERT_EQ(stream->buffer_size(),
                                                    32771lu);
                                          ASSERT_FALSE(stream->CanReadMore());
                                        }));
                              },
                              impl_ptr));
                    },
                    impl_ptr));
          },
          impl_.get()));

  task_environment_.RunUntilIdle();
}

TEST_F(HlsDataSourceProviderImplUnittest, TestAbortMidDownload) {
  // Pregenerating the mock requires setting all our own expectations.
  auto* mock_data_source = factory_->PregenerateNextMock();
  EXPECT_CALL(*mock_data_source, Initialize).WillOnce(RunOnceCallback<0>(true));
  EXPECT_CALL(*mock_data_source, Abort()).Times(0);
  EXPECT_CALL(*mock_data_source, Stop()).Times(0);

  DataSource::ReadCB read_cb;
  EXPECT_CALL(*mock_data_source, Read(0, _, _, _))
      .WillOnce(
          [&read_cb](int64_t, int, uint8_t*, DataSource::ReadCB cb) {
            read_cb = std::move(cb);
          });

  // The Read CB is captured, and so will not execute right away.
  bool has_been_read = false;
  impl_->ReadFromUrl(
      {GURL("example.com"), std::nullopt},
      base::BindOnce(
          [](bool* read_canary, HlsDataSourceProvider::ReadResult result) {
            *read_canary = true;
          },
          &has_been_read));

  // cycle everything and check that we are blocking the read.
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(has_been_read);
  ASSERT_TRUE(!!read_cb);

  // Deleting the HlsDataSourceproviderImpl will stop all existing reads.
  EXPECT_CALL(*mock_data_source, Stop());
  RecreateImpl();
  task_environment_.RunUntilIdle();

  // Run with aborted signal.
  std::move(read_cb).Run(-2);

  task_environment_.RunUntilIdle();
  ASSERT_TRUE(has_been_read);
}

TEST_F(HlsDataSourceProviderImplUnittest, AbortMidInit) {
  auto* mock_data_source = factory_->PregenerateNextMock();

  // Don't run init cb!
  EXPECT_CALL(*mock_data_source, Initialize);
  EXPECT_CALL(*mock_data_source, Stop());

  bool has_been_read = false;
  impl_->ReadFromUrl(
      {GURL("example.com"), std::nullopt},
      base::BindOnce(
          [](bool* read_canary, HlsDataSourceProvider::ReadResult result) {
            *read_canary = true;
          },
          &has_been_read));

  // Despite the init never returning, it is stored in the `data_source_map_`
  // and all entries there get stopped on teardown.
  task_environment_.RunUntilIdle();

  // Should be false, because the stream init function won't post it's callback.
  ASSERT_FALSE(has_been_read);
}

TEST_F(HlsDataSourceProviderImplUnittest, ReadMultipleSegments) {
  HlsDataSourceProvider::SegmentQueue segments;
  segments.emplace(GURL("example.com"), std::nullopt);
  segments.emplace(GURL("foo.com"), std::nullopt);

  // Request 16k, but only 4k is read. Another read then happens and the 0 byte
  // EOS read happens.
  factory_->AddReadExpectation(0, 16384, 4096);
  factory_->AddReadExpectation(4096, 16384, 0);

  std::unique_ptr<HlsDataSourceStream> read_result;
  impl_->ReadFromCombinedUrlQueue(
      std::move(segments), base::BindOnce(
                               [](std::unique_ptr<HlsDataSourceStream>* extract,
                                  HlsDataSourceProvider::ReadResult result) {
                                 *extract = std::move(result).value();
                               },
                               &read_result));
  task_environment_.RunUntilIdle();
  ASSERT_NE(read_result, nullptr);
  ASSERT_TRUE(read_result->CanReadMore());
  ASSERT_FALSE(read_result->RequiresNextDataSource());

  impl_->ReadFromExistingStream(
      std::move(read_result),
      base::BindOnce(
          [](std::unique_ptr<HlsDataSourceStream>* extract,
             HlsDataSourceProvider::ReadResult result) {
            *extract = std::move(result).value();
          },
          &read_result));
  task_environment_.RunUntilIdle();
  ASSERT_NE(read_result, nullptr);
  ASSERT_TRUE(read_result->CanReadMore());
  ASSERT_TRUE(read_result->RequiresNextDataSource());

  factory_->AddReadExpectation(0, 16384, 4096);
  factory_->AddReadExpectation(4096, 16384, 0);
  impl_->ReadFromExistingStream(
      std::move(read_result),
      base::BindOnce(
          [](std::unique_ptr<HlsDataSourceStream>* extract,
             HlsDataSourceProvider::ReadResult result) {
            *extract = std::move(result).value();
          },
          &read_result));

  task_environment_.RunUntilIdle();
  ASSERT_NE(read_result, nullptr);
  ASSERT_TRUE(read_result->CanReadMore());
  ASSERT_FALSE(read_result->RequiresNextDataSource());

  impl_->ReadFromExistingStream(
      std::move(read_result),
      base::BindOnce(
          [](std::unique_ptr<HlsDataSourceStream>* extract,
             HlsDataSourceProvider::ReadResult result) {
            *extract = std::move(result).value();
          },
          &read_result));

  task_environment_.RunUntilIdle();
  ASSERT_NE(read_result, nullptr);
  ASSERT_FALSE(read_result->CanReadMore());
  ASSERT_FALSE(read_result->RequiresNextDataSource());

  read_result = nullptr;
  task_environment_.RunUntilIdle();
}

TEST_F(HlsDataSourceProviderImplUnittest, ReadMultipleRangedSegments) {
  HlsDataSourceProvider::SegmentQueue segments;
  // Read 10 bytes from offset 0.
  segments.emplace(GURL("example.com"), hls::types::ByteRange::Validate(10, 0));

  // Read 100 bytes from offset 100
  segments.emplace(GURL("foo.com"), hls::types::ByteRange::Validate(100, 100));

  // Request 10 bytes from the 0 offset. this is fairly common for EXT-X-MAP.
  factory_->AddReadExpectation(0, 10, 10);

  std::unique_ptr<HlsDataSourceStream> read_result;
  impl_->ReadFromCombinedUrlQueue(
      std::move(segments), base::BindOnce(
                               [](std::unique_ptr<HlsDataSourceStream>* extract,
                                  HlsDataSourceProvider::ReadResult result) {
                                 *extract = std::move(result).value();
                               },
                               &read_result));

  task_environment_.RunUntilIdle();
  ASSERT_NE(read_result, nullptr);
  ASSERT_TRUE(read_result->CanReadMore());
  ASSERT_TRUE(read_result->RequiresNextDataSource());

  // The second segment will request another 100 bytes, and again, because it is
  // a range request, more should be returned.
  factory_->AddReadExpectation(100, 100, 100);
  impl_->ReadFromExistingStream(
      std::move(read_result),
      base::BindOnce(
          [](std::unique_ptr<HlsDataSourceStream>* extract,
             HlsDataSourceProvider::ReadResult result) {
            *extract = std::move(result).value();
          },
          &read_result));

  task_environment_.RunUntilIdle();
  ASSERT_NE(read_result, nullptr);
  ASSERT_FALSE(read_result->CanReadMore());
  ASSERT_FALSE(read_result->RequiresNextDataSource());

  read_result = nullptr;
  task_environment_.RunUntilIdle();
}

}  // namespace blink
