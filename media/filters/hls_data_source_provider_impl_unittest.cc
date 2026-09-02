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
      {GURL("https://example.com"), std::nullopt},
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
      {GURL("https://example.com"), std::nullopt},
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
      {GURL("https://example.com"), hls::types::ByteRange::Validate(4242, 99)},
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
      {GURL("https://example.com"), std::nullopt},
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
  DataSource::ReadCB read_cb;
  MockDataSource* mock_ds;
  MockDataSource** write_out = &mock_ds;
  EXPECT_CALL(*factory_, Setup(_, GURL("https://example.com"), _, _))
      .WillOnce([&read_cb, &write_out](auto* data_source, const auto&, ...) {
        EXPECT_CALL(*data_source, Initialize)
            .WillOnce(RunOnceCallback<0>(true));
        EXPECT_CALL(*data_source, GetUrlDataOrigin())
            .WillOnce(testing::Return(GURL("https://example.com")));
        EXPECT_CALL(*data_source, Stop()).Times(0);
        EXPECT_CALL(*data_source, Abort()).Times(0);
        EXPECT_CALL(*data_source, Read(0, _, _))
            .WillOnce(
                [&read_cb](auto, auto, auto cb) { read_cb = std::move(cb); });
        *write_out = data_source;
      });

  // The Read CB is captured, and so will not execute right away.
  bool has_been_read = false;
  impl_->ReadFromUrl(
      {GURL("https://example.com"), std::nullopt},
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
  EXPECT_CALL(*mock_ds, Stop());
  RecreateImpl();
  task_environment_.RunUntilIdle();

  // Run with aborted signal.
  std::move(read_cb).Run(-2);

  task_environment_.RunUntilIdle();
  ASSERT_TRUE(has_been_read);
}

TEST_F(HlsDataSourceProviderImplUnittest, AbortMidInit) {
  EXPECT_CALL(*factory_, Setup(_, GURL("https://example.com"), _, _))
      .WillOnce([](MockDataSource* mock, const GURL&, ...) {
        // Don't run init cb!
        EXPECT_CALL(*mock, Initialize);
        EXPECT_CALL(*mock, Stop());
      });

  bool has_been_read = false;
  impl_->ReadFromUrl(
      {GURL("https://example.com"), std::nullopt},
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

TEST_F(HlsDataSourceProviderImplUnittest, ReadSegmentWithRedirect) {
  HlsDataSourceProvider::UrlDataSegment segment(GURL("http://evil.com"),
                                                std::nullopt);

  {
    MockDataSourceFactory::DataSourceBehavior params;
    params.would_taint_origin = true;
    params.original_uri = GURL("http://evil.com");
    params.redirect_uri = "http://innocent.com";
    params.read_expectations = {
        // Request 16k, but only 4k is read. Another read then happens and the
        // 0 byte EOS read happens.
        std::make_tuple(0, 16384, 4096),
        std::make_tuple(4096, 16384, 0),
    };
    factory_->Expect(std::move(params));
  }

  std::unique_ptr<HlsDataSourceStream> first_read;
  impl_->ReadFromUrl(std::move(segment),
                     base::BindOnce(
                         [](std::unique_ptr<HlsDataSourceStream>* extract,
                            HlsDataSourceProvider::ReadResult result) {
                           *extract = std::move(result).value();
                         },
                         &first_read));
  task_environment_.RunUntilIdle();
  ASSERT_NE(first_read, nullptr);
  ASSERT_TRUE(first_read->CanReadMore());
  ASSERT_FALSE(first_read->RequiresInit());

  std::unique_ptr<HlsDataSourceStream> second_read;
  impl_->ReadFromExistingStream(
      std::move(first_read),
      base::BindOnce(
          [](std::unique_ptr<HlsDataSourceStream>* extract,
             HlsDataSourceProvider::ReadResult result) {
            *extract = std::move(result).value();
          },
          &second_read));
  task_environment_.RunUntilIdle();
  ASSERT_NE(second_read, nullptr);
  ASSERT_FALSE(second_read->CanReadMore());
  ASSERT_FALSE(second_read->RequiresInit());
  ASSERT_EQ(second_read->SecurityInfo().response_origins.size(), 1u);
  ASSERT_EQ(*second_read->SecurityInfo().response_origins.begin(),
            url::Origin::Create(GURL("http://innocent.com")));

  second_read = nullptr;
  task_environment_.RunUntilIdle();
}

TEST_F(HlsDataSourceProviderImplUnittest, TestCrossOriginRangeRequest) {
  HlsDataSourceProvider::UrlDataSegment segment(
      GURL("http://example.com"), hls::types::ByteRange::Validate(10, 0));

  const GURL url("http://example.com");
  EXPECT_CALL(*factory_, Setup(_, url, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        EXPECT_CALL(*mock, WouldTaintOrigin())
            .WillOnce(Return(false))
            .WillOnce(Return(true));
      });

  bool has_error = false;
  impl_->ReadFromUrl(
      std::move(segment),
      base::BindOnce(
          [](bool* error_canary, HlsDataSourceProvider::ReadResult result) {
            if (!result.has_value()) {
              *error_canary =
                  (std::move(result).error() ==
                   HlsDataSourceProvider::ReadStatus::Codes::kError);
            }
          },
          &has_error));

  task_environment_.RunUntilIdle();
  ASSERT_TRUE(has_error);
}

TEST_F(HlsDataSourceProviderImplUnittest, ReadSegmentWithDifferentDataOrigin) {
  HlsDataSourceProvider::UrlDataSegment segment(GURL("http://evil.com"),
                                                std::nullopt);

  const GURL url("http://evil.com");
  EXPECT_CALL(*factory_, Setup(_, url, _, _))
      .WillOnce([](MockDataSource* mock, const GURL& uri, ...) {
        MockDataSourceFactory::ConfigureAsSuccess(mock, uri);
        ON_CALL(*mock, GetUrlAfterRedirects())
            .WillByDefault(testing::Return(GURL("http://evil.com")));
        ON_CALL(*mock, GetUrlDataOrigin())
            .WillByDefault(testing::Return(GURL("http://innocent.com")));
        EXPECT_CALL(*mock, Read(0, SpanSizeEq(16384), _))
            .WillOnce(base::test::RunOnceCallback<2>(4096));
      });

  std::unique_ptr<HlsDataSourceStream> first_read;
  impl_->ReadFromUrl(std::move(segment),
                     base::BindOnce(
                         [](std::unique_ptr<HlsDataSourceStream>* extract,
                            HlsDataSourceProvider::ReadResult result) {
                           *extract = std::move(result).value();
                         },
                         &first_read));
  task_environment_.RunUntilIdle();
  ASSERT_NE(first_read, nullptr);
  ASSERT_TRUE(first_read->CanReadMore());
  ASSERT_FALSE(first_read->RequiresInit());
  ASSERT_EQ(first_read->SecurityInfo().response_origins.size(), 1u);
  ASSERT_EQ(*first_read->SecurityInfo().response_origins.begin(),
            url::Origin::Create(GURL("http://innocent.com")));
}

}  // namespace media
