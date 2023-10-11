// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "media/base/mock_media_log.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/media/buffered_data_source_host_impl.h"
#include "third_party/blink/renderer/platform/media/hls_data_source_provider_impl.h"
#include "third_party/blink/renderer/platform/media/multi_buffer_data_source.h"

namespace blink {

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

namespace {

class MockDataSource : public media::CrossOriginDataSource {
 public:
  // Mocked methods from CrossOriginDataSource
  MOCK_METHOD(bool, IsCorsCrossOrigin, (), (const, override));
  MOCK_METHOD(bool, HasAccessControl, (), (const, override));
  MOCK_METHOD(const std::string&, GetMimeType, (), (const, override));
  MOCK_METHOD(void,
              Initialize,
              (base::OnceCallback<void(bool)> init_cb),
              (override));

  // Mocked methods from DataSource
  MOCK_METHOD(
      void,
      Read,
      (int64_t position, int size, uint8_t* data, DataSource::ReadCB read_cb),
      (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, Abort, (), (override));
  MOCK_METHOD(bool, GetSize, (int64_t * size_out), (override));
  MOCK_METHOD(bool, IsStreaming, (), (override));
  MOCK_METHOD(void, SetBitrate, (int bitrate), (override));
  MOCK_METHOD(bool, PassedTimingAllowOriginCheck, (), (override));
  MOCK_METHOD(bool, WouldTaintOrigin, (), (override));
  MOCK_METHOD(bool, AssumeFullyBuffered, (), (const, override));
  MOCK_METHOD(int64_t, GetMemoryUsage, (), (override));
  MOCK_METHOD(void,
              SetPreload,
              (media::DataSource::Preload preload),
              (override));
  MOCK_METHOD(GURL, GetUrlAfterRedirects, (), (const, override));
  MOCK_METHOD(void,
              OnBufferingHaveEnough,
              (bool must_cancel_netops),
              (override));
  MOCK_METHOD(void,
              OnMediaPlaybackRateChanged,
              (double playback_rate),
              (override));
  MOCK_METHOD(void, OnMediaIsPlaying, (), (override));
  MOCK_METHOD(const CrossOriginDataSource*,
              GetAsCrossOriginDataSource,
              (),
              (const, override));
};

class MockDataSourceFactory
    : public HlsDataSourceProviderImpl::DataSourceFactory {
 public:
  using MockDataSource = StrictMock<MockDataSource>;

  ~MockDataSourceFactory() override = default;
  MockDataSourceFactory() = default;
  void CreateDataSource(GURL uri, DataSourceCb cb) override {
    if (!next_mock_) {
      PregenerateNextMock();
      EXPECT_CALL(*next_mock_, Initialize).WillOnce(RunOnceCallback<0>(true));
      for (const auto& e : read_expectations_) {
        EXPECT_CALL(*next_mock_, Read(std::get<0>(e), std::get<1>(e), _, _))
            .WillOnce(RunOnceCallback<3>(std::get<2>(e)));
      }
      read_expectations_.clear();
      EXPECT_CALL(*next_mock_, Abort());
      EXPECT_CALL(*next_mock_, Stop());
    }
    std::move(cb).Run(std::move(next_mock_));
  }

  void AddReadExpectation(size_t from, size_t to, int response) {
    read_expectations_.emplace_back(from, to, response);
  }

  MockDataSource* PregenerateNextMock() {
    next_mock_ = std::make_unique<MockDataSource>();
    return next_mock_.get();
  }

 private:
  std::unique_ptr<MockDataSource> next_mock_;
  std::vector<std::tuple<size_t, size_t, int>> read_expectations_;
};

}  // namespace

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
      GURL("example.com"), absl::nullopt,
      base::BindOnce([](media::HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_EQ(stream->read_position(), 16384lu);
        ASSERT_EQ(stream->buffer_size(), 16384lu);
        ASSERT_EQ(stream->max_read_position(), absl::nullopt);
        ASSERT_TRUE(stream->CanReadMore());
      }));
  task_environment_.RunUntilIdle();

  // Only got 400 bytes of requested, which means that there is no more to read.
  factory_->AddReadExpectation(0, 16384, 400);
  impl_->ReadFromUrl(
      GURL("example.com"), absl::nullopt,
      base::BindOnce([](media::HlsDataSourceProvider::ReadResult result) {
        ASSERT_TRUE(result.has_value());
        auto stream = std::move(result).value();
        ASSERT_EQ(stream->read_position(), 400lu);
        ASSERT_EQ(stream->buffer_size(), 400lu);
        ASSERT_EQ(stream->max_read_position(), absl::nullopt);
        ASSERT_FALSE(stream->CanReadMore());
      }));
  task_environment_.RunUntilIdle();

  // The data source should only be limited to 4242 total bytes and should start
  // at an offset of 99. The read should be from 99, size of 4242.
  factory_->AddReadExpectation(99, 4242, 4242);
  impl_->ReadFromUrl(
      GURL("example.com"), media::hls::types::ByteRange::Validate(4242, 99),
      base::BindOnce([](media::HlsDataSourceProvider::ReadResult result) {
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
  impl_->ReadFromUrl(
      GURL("example.com"), absl::nullopt,
      base::BindOnce(
          [](HlsDataSourceProviderImpl* impl_ptr,
             media::HlsDataSourceProvider::ReadResult result) {
            ASSERT_TRUE(result.has_value());
            auto stream = std::move(result).value();
            ASSERT_EQ(stream->read_position(), 16384lu);
            ASSERT_EQ(stream->buffer_size(), 16384lu);
            ASSERT_TRUE(stream->CanReadMore());

            impl_ptr->ReadFromExistingStream(
                std::move(stream),
                base::BindOnce(
                    [](HlsDataSourceProviderImpl* impl_ptr,
                       media::HlsDataSourceProvider::ReadResult result) {
                      ASSERT_TRUE(result.has_value());
                      auto stream = std::move(result).value();
                      ASSERT_EQ(stream->read_position(), 32768lu);
                      ASSERT_EQ(stream->buffer_size(), 32768lu);
                      ASSERT_TRUE(stream->CanReadMore());

                      impl_ptr->ReadFromExistingStream(
                          std::move(stream),
                          base::BindOnce(
                              [](media::HlsDataSourceProvider::ReadResult
                                     result) {
                                ASSERT_TRUE(result.has_value());
                                auto stream = std::move(result).value();
                                ASSERT_EQ(stream->read_position(), 32771lu);
                                ASSERT_EQ(stream->buffer_size(), 32771lu);
                                ASSERT_FALSE(stream->CanReadMore());
                              }));
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

  media::DataSource::ReadCB read_cb;
  EXPECT_CALL(*mock_data_source, Read(0, _, _, _))
      .WillOnce(
          [&read_cb](int64_t, int, uint8_t*, media::DataSource::ReadCB cb) {
            read_cb = std::move(cb);
          });

  // The Read CB is captured, and so will not execute right away.
  bool has_been_read = false;
  impl_->ReadFromUrl(GURL("example.com"), absl::nullopt,
                     base::BindOnce(
                         [](bool* read_canary,
                            media::HlsDataSourceProvider::ReadResult result) {
                           *read_canary = true;
                         },
                         &has_been_read));

  // cycle everything and check that we are blocking the read.
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(has_been_read);
  ASSERT_TRUE(!!read_cb);

  // Deleting the HlsDataSourceproviderImpl will abort all existing reads.
  EXPECT_CALL(*mock_data_source, Abort());
  EXPECT_CALL(*mock_data_source, Stop());
  RecreateImpl();
  task_environment_.RunUntilIdle();

  // Run with aborted signal.
  std::move(read_cb).Run(-2);

  task_environment_.RunUntilIdle();
  ASSERT_TRUE(has_been_read);
}

}  // namespace blink
