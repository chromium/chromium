// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_url_demuxer.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "net/cookies/site_for_cookies.h"
#include "net/storage_access_api/status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MediaUrlDemuxerTest : public testing::Test {
 public:
  MediaUrlDemuxerTest()
      : default_media_url_(GURL("http://example.com/file.mp4")),
        default_first_party_url_(GURL("http://example.com/")) {}

  MediaUrlDemuxerTest(const MediaUrlDemuxerTest&) = delete;
  MediaUrlDemuxerTest& operator=(const MediaUrlDemuxerTest&) = delete;

  void InitializeTest(const GURL& media_url,
                      const GURL& first_party,
                      bool allow_credentials) {
    demuxer_ = std::make_unique<MediaUrlDemuxer>(
        base::SingleThreadTaskRunner::GetCurrentDefault(), media_url,
        net::SiteForCookies::FromUrl(first_party),
        url::Origin::Create(first_party), net::StorageAccessApiStatus::kNone,
        allow_credentials, false);
  }

  void InitializeTest() {
    InitializeTest(default_media_url_, default_first_party_url_, true);
  }

  void VerifyCallbackOk(PipelineStatus status) {
    EXPECT_EQ(PIPELINE_OK, status);
  }

  const GURL default_media_url_;
  const GURL default_first_party_url_;
  std::unique_ptr<Demuxer> demuxer_;

  // Necessary, or else base::SingleThreadTaskRunner::GetCurrentDefault() fails.
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(MediaUrlDemuxerTest, BaseCase) {
  InitializeTest();

  EXPECT_EQ(MediaResource::Type::KUrl, demuxer_->GetType());

  const MediaUrlParams& params = demuxer_->GetMediaUrlParams();
  EXPECT_EQ(default_media_url_, params.media_url);
  EXPECT_TRUE(net::SiteForCookies::FromUrl(default_first_party_url_)
                  .IsEquivalent(params.site_for_cookies));
  EXPECT_EQ(true, params.allow_credentials);
}

TEST_F(MediaUrlDemuxerTest, AcceptsEmptyStrings) {
  InitializeTest(GURL(), GURL(), false);

  const MediaUrlParams& params = demuxer_->GetMediaUrlParams();
  EXPECT_EQ(GURL(), params.media_url);
  EXPECT_TRUE(net::SiteForCookies::FromUrl(GURL()).IsEquivalent(
      params.site_for_cookies));
  EXPECT_EQ(false, params.allow_credentials);
}

TEST_F(MediaUrlDemuxerTest, InitializeReturnsPipelineOk) {
  InitializeTest();
  demuxer_->Initialize(nullptr,
                       base::BindOnce(&MediaUrlDemuxerTest::VerifyCallbackOk,
                                      base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaUrlDemuxerTest, SeekReturnsPipelineOk) {
  InitializeTest();
  demuxer_->Seek(base::TimeDelta(),
                 base::BindOnce(&MediaUrlDemuxerTest::VerifyCallbackOk,
                                base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
}

}  // namespace media
