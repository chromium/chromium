// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/text_renderer.h"

#include <stddef.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/fake_text_track_stream.h"
#include "media/base/text_track_config.h"
#include "media/base/video_decoder_config.h"
#include "media/base/webvtt_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Local implementation of the TextTrack interface.
class FakeTextTrack : public TextTrack {
 public:
  FakeTextTrack(const base::Closure& destroy_cb, const TextTrackConfig& config)
      : destroy_cb_(destroy_cb), config_(config) {}
  ~FakeTextTrack() override { destroy_cb_.Run(); }

  MOCK_METHOD5(addWebVTTCue,
               void(base::TimeDelta start,
                    base::TimeDelta end,
                    const std::string& id,
                    const std::string& content,
                    const std::string& settings));

  const base::Closure destroy_cb_;
  const TextTrackConfig config_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeTextTrack);
};

class TextRendererTest : public testing::Test {
 public:
  TextRendererTest() = default;

  void CreateTextRenderer() {
    DCHECK(!text_renderer_);

    text_renderer_.reset(new TextRenderer(
        task_environment_.GetMainThreadTaskRunner(),
        base::Bind(&TextRendererTest::OnAddTextTrack, base::Unretained(this))));
    text_renderer_->Initialize(
        base::Bind(&TextRendererTest::OnEnd, base::Unretained(this)));
  }

  void Destroy() {
    text_renderer_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void AddTextTrack(TextKind kind,
                    const std::string& name,
                    const std::string& language,
                    bool expect_read) {
    const size_t idx = text_track_streams_.size();
    text_track_streams_.push_back(std::make_unique<FakeTextTrackStream>());

    if (expect_read)
      ExpectRead(idx);

    const TextTrackConfig config(kind, name, language, std::string());
    text_renderer_->AddTextStream(text_track_streams_.back().get(), config);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(text_tracks_.size(), text_track_streams_.size());
    FakeTextTrack* const text_track = text_tracks_.back();
    EXPECT_TRUE(text_track);
    EXPECT_TRUE(text_track->config_.Matches(config));
  }

  void OnAddTextTrack(const TextTrackConfig& config,
                      const AddTextTrackDoneCB& done_cb) {
    base::Closure destroy_cb =
        base::Bind(&TextRendererTest::OnDestroyTextTrack,
                   base::Unretained(this), text_tracks_.size());
    // Text track objects are owned by the text renderer, but we cache them
    // here so we can inspect them.  They get removed from our cache when the
    // text renderer deallocates them.
    text_tracks_.push_back(new FakeTextTrack(destroy_cb, config));
    std::unique_ptr<TextTrack> text_track(text_tracks_.back());
    done_cb.Run(std::move(text_track));
  }

  void RemoveTextTrack(unsigned idx) {
    FakeTextTrackStream* const stream = text_track_streams_[idx].get();
    text_renderer_->RemoveTextStream(stream);
    EXPECT_FALSE(text_tracks_[idx]);
  }

  void SatisfyPendingReads(base::TimeDelta start,
                           base::TimeDelta duration,
                           const std::string& id,
                           const std::string& content,
                           const std::string& settings) {
    for (auto itr = text_track_streams_.begin();
         itr != text_track_streams_.end(); ++itr) {
      (*itr)->SatisfyPendingRead(start, duration, id, content, settings);
    }
  }

  void AbortPendingRead(unsigned idx) {
    FakeTextTrackStream* const stream = text_track_streams_[idx].get();
    stream->AbortPendingRead();
    base::RunLoop().RunUntilIdle();
  }

  void AbortPendingReads() {
    for (size_t idx = 0; idx < text_track_streams_.size(); ++idx) {
      AbortPendingRead(idx);
    }
  }

  void SendEosNotification(unsigned idx) {
    FakeTextTrackStream* const stream = text_track_streams_[idx].get();
    stream->SendEosNotification();
    base::RunLoop().RunUntilIdle();
  }

  void SendEosNotifications() {
    for (size_t idx = 0; idx < text_track_streams_.size(); ++idx) {
      SendEosNotification(idx);
    }
  }

  void SendCue(unsigned idx, bool expect_cue) {
    FakeTextTrackStream* const text_stream = text_track_streams_[idx].get();

    const base::TimeDelta start;
    const base::TimeDelta duration = base::TimeDelta::FromSeconds(42);
    const std::string id = "id";
    const std::string content = "subtitle";
    const std::string settings;

    if (expect_cue) {
      FakeTextTrack* const text_track = text_tracks_[idx];
      EXPECT_CALL(*text_track,
                  addWebVTTCue(start, start + duration, id, content, settings));
    }

    text_stream->SatisfyPendingRead(start, duration, id, content, settings);
    base::RunLoop().RunUntilIdle();
  }

  void SendCues(bool expect_cue) {
    for (size_t idx = 0; idx < text_track_streams_.size(); ++idx) {
      SendCue(idx, expect_cue);
    }
  }

  void OnDestroyTextTrack(unsigned idx) { text_tracks_[idx] = NULL; }

  void Play() { text_renderer_->StartPlaying(); }

  void Pause() {
    text_renderer_->Pause(
        base::Bind(&TextRendererTest::OnPause, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void Flush() {
    EXPECT_CALL(*this, OnFlush());
    text_renderer_->Flush(
        base::Bind(&TextRendererTest::OnFlush, base::Unretained(this)));
  }

  void ExpectRead(size_t idx) {
    FakeTextTrackStream* const stream = text_track_streams_[idx].get();
    EXPECT_CALL(*stream, OnRead());
  }

  MOCK_METHOD0(OnEnd, void());
  MOCK_METHOD0(OnPause, void());
  MOCK_METHOD0(OnFlush, void());

  base::test::SingleThreadTaskEnvironment task_environment_;

  typedef std::vector<std::unique_ptr<FakeTextTrackStream>> TextTrackStreams;
  TextTrackStreams text_track_streams_;

  typedef std::vector<FakeTextTrack*> TextTracks;
  TextTracks text_tracks_;

  std::unique_ptr<TextRenderer> text_renderer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextRendererTest);
};

TEST_F(TextRendererTest, CreateTextRendererNoInit) {
  text_renderer_.reset(new TextRenderer(
      task_environment_.GetMainThreadTaskRunner(),
      base::Bind(&TextRendererTest::OnAddTextTrack, base::Unretained(this))));
  text_renderer_.reset();
}

TEST_F(TextRendererTest, Create) {
  CreateTextRenderer();
}

TEST_F(TextRendererTest, AddTextTrackOnly_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", false);
}

TEST_F(TextRendererTest, AddTextTrackOnly_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "track 1", "", false);
  AddTextTrack(kTextSubtitles, "track 2", "", false);
}

TEST_F(TextRendererTest, PlayOnly) {
  CreateTextRenderer();
  Play();
}

TEST_F(TextRendererTest, AddTrackBeforePlay_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  AbortPendingReads();
}

TEST_F(TextRendererTest, AddTrackBeforePlay_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  AbortPendingReads();
}

TEST_F(TextRendererTest, AddTrackAfterPlay_OneTrackAfter) {
  CreateTextRenderer();
  Play();
  AddTextTrack(kTextSubtitles, "", "", true);
  AbortPendingReads();
}

TEST_F(TextRendererTest, AddTrackAfterPlay_TwoTracksAfter) {
  CreateTextRenderer();
  Play();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  AbortPendingReads();
}

TEST_F(TextRendererTest, AddTrackAfterPlay_OneTrackBeforeOneTrackAfter) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  Play();
  AddTextTrack(kTextSubtitles, "2", "", true);
  AbortPendingReads();
}

TEST_F(TextRendererTest, PlayAddCue_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  ExpectRead(0);
  SendCues(true);
  AbortPendingReads();
}

TEST_F(TextRendererTest, PlayAddCue_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  ExpectRead(0);
  ExpectRead(1);
  SendCues(true);
  AbortPendingReads();
}

TEST_F(TextRendererTest, PlayEosOnly_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
}

TEST_F(TextRendererTest, PlayEosOnly_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
}

TEST_F(TextRendererTest, PlayCueEos_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  ExpectRead(0);
  SendCues(true);
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
}

TEST_F(TextRendererTest, PlayCueEos_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  ExpectRead(0);
  ExpectRead(1);
  SendCues(true);
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
}

TEST_F(TextRendererTest, DestroyPending_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  Destroy();
  SendEosNotifications();
}

TEST_F(TextRendererTest, DestroyPending_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  Destroy();
  SendEosNotifications();
}

TEST_F(TextRendererTest, PlayPause_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  AbortPendingReads();
  EXPECT_CALL(*this, OnPause());
  Pause();
}

TEST_F(TextRendererTest, PlayPause_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  AbortPendingReads();
  EXPECT_CALL(*this, OnPause());
  Pause();
}

TEST_F(TextRendererTest, PlayEosPausePending_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  Pause();
  EXPECT_CALL(*this, OnPause());
  SendEosNotifications();
}

TEST_F(TextRendererTest, PlayEosPausePending_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  Pause();
  EXPECT_CALL(*this, OnPause());
  SendEosNotifications();
}

TEST_F(TextRendererTest, PlayCuePausePending_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  Pause();
  EXPECT_CALL(*this, OnPause());
  SendCues(true);
}

TEST_F(TextRendererTest, PlayCuePausePending_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  Pause();
  EXPECT_CALL(*this, OnPause());
  SendCues(true);
}

TEST_F(TextRendererTest, PlayEosPause_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
  EXPECT_CALL(*this, OnPause());
  Pause();
}

TEST_F(TextRendererTest, PlayEosPause_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
  EXPECT_CALL(*this, OnPause());
  Pause();
}

TEST_F(TextRendererTest, PlayEosPause_SplitEos) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  SendEosNotification(0);
  EXPECT_CALL(*this, OnEnd());
  SendEosNotification(1);
  EXPECT_CALL(*this, OnPause());
  Pause();
}

TEST_F(TextRendererTest, PlayEosFlush_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
  EXPECT_CALL(*this, OnPause());
  Pause();
  Flush();
  ExpectRead(0);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
}

TEST_F(TextRendererTest, PlayEosFlush_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
  EXPECT_CALL(*this, OnPause());
  Pause();
  Flush();
  ExpectRead(0);
  ExpectRead(1);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
}

TEST_F(TextRendererTest, AddTextTrackOnlyRemove_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", false);
  EXPECT_TRUE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, AddTextTrackOnlyRemove_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "track 1", "", false);
  AddTextTrack(kTextSubtitles, "track 2", "", false);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, AddTrackBeforePlayRemove_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  AbortPendingReads();
  RemoveTextTrack(0);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, AddTrackBeforePlayRemove_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  AbortPendingReads();
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, AddTrackBeforePlayRemove_SeparateCancel) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  AbortPendingRead(0);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  AbortPendingRead(1);
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, AddTrackBeforePlayRemove_RemoveOneThenPlay) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", false);
  AddTextTrack(kTextSubtitles, "2", "", true);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  Play();
  AbortPendingRead(1);
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, AddTrackBeforePlayRemove_RemoveTwoThenPlay) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", false);
  AddTextTrack(kTextSubtitles, "2", "", false);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
  Play();
}

TEST_F(TextRendererTest, AddTrackAfterPlayRemove_OneTrack) {
  CreateTextRenderer();
  Play();
  AddTextTrack(kTextSubtitles, "", "", true);
  AbortPendingReads();
  RemoveTextTrack(0);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, AddTrackAfterPlayRemove_TwoTracks) {
  CreateTextRenderer();
  Play();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  AbortPendingReads();
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, AddTrackAfterPlayRemove_SplitCancel) {
  CreateTextRenderer();
  Play();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  AbortPendingRead(0);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  AbortPendingRead(1);
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, AddTrackAfterPlayRemove_SplitAdd) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  Play();
  AddTextTrack(kTextSubtitles, "2", "", true);
  AbortPendingRead(0);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  AbortPendingRead(1);
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayAddCueRemove_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  ExpectRead(0);
  SendCues(true);
  AbortPendingReads();
  RemoveTextTrack(0);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayAddCueRemove_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  ExpectRead(0);
  ExpectRead(1);
  SendCues(true);
  AbortPendingRead(0);
  AbortPendingRead(1);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayEosOnlyRemove_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
  RemoveTextTrack(0);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayEosOnlyRemove_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayCueEosRemove_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  ExpectRead(0);
  SendCues(true);
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
  RemoveTextTrack(0);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayCueEosRemove_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  ExpectRead(0);
  ExpectRead(1);
  SendCues(true);
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayPauseRemove_PauseThenRemove) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  AbortPendingReads();
  EXPECT_CALL(*this, OnPause());
  Pause();
  RemoveTextTrack(0);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayPauseRemove_RemoveThanPause) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  AbortPendingReads();
  RemoveTextTrack(0);
  EXPECT_FALSE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnPause());
  Pause();
}

TEST_F(TextRendererTest, PlayPause_PauseThenRemoveTwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  AbortPendingReads();
  EXPECT_CALL(*this, OnPause());
  Pause();
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayPauseRemove_RemoveThenPauseTwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  AbortPendingReads();
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnPause());
  Pause();
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayPauseRemove_SplitCancel) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  AbortPendingRead(0);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  AbortPendingRead(1);
  EXPECT_CALL(*this, OnPause());
  Pause();
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayPauseRemove_PauseLast) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  AbortPendingRead(0);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  AbortPendingRead(1);
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnPause());
  Pause();
}

TEST_F(TextRendererTest, PlayEosPausePendingRemove_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  Pause();
  EXPECT_CALL(*this, OnPause());
  SendEosNotifications();
  RemoveTextTrack(0);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayEosPausePendingRemove_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  Pause();
  SendEosNotification(0);
  EXPECT_CALL(*this, OnPause());
  SendEosNotification(1);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayEosPausePendingRemove_SplitEos) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  Pause();
  SendEosNotification(0);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnPause());
  SendEosNotification(1);
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayCuePausePendingRemove_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  Pause();
  EXPECT_CALL(*this, OnPause());
  SendCues(true);
  RemoveTextTrack(0);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayCuePausePendingRemove_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  Pause();
  SendCue(0, true);
  EXPECT_CALL(*this, OnPause());
  SendCue(1, true);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayCuePausePendingRemove_SplitSendCue) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  Pause();
  SendCue(0, true);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnPause());
  SendCue(1, true);
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayEosPauseRemove_PauseThenRemove) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
  EXPECT_CALL(*this, OnPause());
  Pause();
  RemoveTextTrack(0);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayEosPauseRemove_RemoveThenPause) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
  RemoveTextTrack(0);
  EXPECT_FALSE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnPause());
  Pause();
}

TEST_F(TextRendererTest, PlayEosPause_PauseThenRemoveTwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  SendEosNotification(0);
  EXPECT_CALL(*this, OnEnd());
  SendEosNotification(1);
  EXPECT_CALL(*this, OnPause());
  Pause();
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayEosPause_RemovePauseRemove) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  SendEosNotification(0);
  EXPECT_CALL(*this, OnEnd());
  SendEosNotification(1);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnPause());
  Pause();
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayEosPause_EosThenPause) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  SendEosNotification(0);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnEnd());
  SendEosNotification(1);
  EXPECT_CALL(*this, OnPause());
  Pause();
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayEosPause_PauseLast) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  SendEosNotification(0);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnEnd());
  SendEosNotification(1);
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnPause());
  Pause();
}

TEST_F(TextRendererTest, PlayEosPause_EosPauseRemove) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  SendEosNotification(0);
  EXPECT_CALL(*this, OnEnd());
  SendEosNotification(1);
  EXPECT_CALL(*this, OnPause());
  Pause();
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayEosPause_EosRemovePause) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  SendEosNotification(0);
  EXPECT_CALL(*this, OnEnd());
  SendEosNotification(1);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnPause());
  Pause();
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayEosPause_EosRemoveEosPause) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  SendEosNotification(0);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnEnd());
  SendEosNotification(1);
  EXPECT_CALL(*this, OnPause());
  Pause();
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayEosPause_EosRemoveEosRemovePause) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  SendEosNotification(0);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnEnd());
  SendEosNotification(1);
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnPause());
  Pause();
}

TEST_F(TextRendererTest, PlayEosFlushRemove_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
  EXPECT_CALL(*this, OnPause());
  Pause();
  Flush();
  ExpectRead(0);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
  RemoveTextTrack(0);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayEosFlushRemove_TwoTracks) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
  EXPECT_CALL(*this, OnPause());
  Pause();
  Flush();
  ExpectRead(0);
  ExpectRead(1);
  Play();
  SendEosNotification(0);
  EXPECT_CALL(*this, OnEnd());
  SendEosNotification(1);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayEosFlushRemove_EosRemove) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotifications();
  EXPECT_CALL(*this, OnPause());
  Pause();
  Flush();
  ExpectRead(0);
  ExpectRead(1);
  Play();
  SendEosNotification(0);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnEnd());
  SendEosNotification(1);
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayShort_SendCueThenEos) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  Pause();
  SendCue(0, true);
  EXPECT_CALL(*this, OnPause());
  SendEosNotification(1);
}

TEST_F(TextRendererTest, PlayShort_EosThenSendCue) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  Pause();
  SendEosNotification(0);
  EXPECT_CALL(*this, OnPause());
  SendCue(1, true);
}

TEST_F(TextRendererTest, PlayShortRemove_SendEosRemove) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  Pause();
  SendCue(0, true);
  EXPECT_CALL(*this, OnPause());
  SendEosNotification(1);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayShortRemove_SendRemoveEos) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  Pause();
  SendCue(0, true);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnPause());
  SendEosNotification(1);
  RemoveTextTrack(1);
  EXPECT_FALSE(text_renderer_->HasTracks());
}

TEST_F(TextRendererTest, PlayCuePausePendingCancel_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  Pause();
  EXPECT_CALL(*this, OnPause());
  AbortPendingRead(0);
}

TEST_F(TextRendererTest, PlayCuePausePendingCancel_SendThenCancel) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  Pause();
  SendCue(0, true);
  EXPECT_CALL(*this, OnPause());
  AbortPendingRead(1);
}

TEST_F(TextRendererTest, PlayCuePausePendingCancel_CancelThenSend) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  Pause();
  AbortPendingRead(0);
  EXPECT_CALL(*this, OnPause());
  SendCue(1, true);
}

TEST_F(TextRendererTest, PlayCueDestroyPendingCancel_OneTrack) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  Pause();
  EXPECT_CALL(*this, OnPause());
  Destroy();
  AbortPendingRead(0);
}

TEST_F(TextRendererTest, PlayCueDestroyPendingCancel_SendThenCancel) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  Pause();
  EXPECT_CALL(*this, OnPause());
  Destroy();
  SendCue(0, false);
  AbortPendingRead(1);
}

TEST_F(TextRendererTest, PlayCueDestroyPendingCancel_CancelThenSend) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  Pause();
  EXPECT_CALL(*this, OnPause());
  Destroy();
  AbortPendingRead(0);
  SendCue(1, false);
}

TEST_F(TextRendererTest, AddRemoveAdd) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  AbortPendingRead(0);
  RemoveTextTrack(0);
  EXPECT_CALL(*this, OnPause());
  Pause();
  AddTextTrack(kTextSubtitles, "", "", true);
  Play();
  EXPECT_CALL(*this, OnEnd());
  SendEosNotification(1);
}

TEST_F(TextRendererTest, AddRemoveEos) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  AbortPendingRead(0);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  EXPECT_CALL(*this, OnEnd());
  SendEosNotification(1);
}

TEST_F(TextRendererTest, AddRemovePause) {
  CreateTextRenderer();
  AddTextTrack(kTextSubtitles, "1", "", true);
  AddTextTrack(kTextSubtitles, "2", "", true);
  Play();
  AbortPendingRead(0);
  RemoveTextTrack(0);
  EXPECT_TRUE(text_renderer_->HasTracks());
  Pause();
  EXPECT_CALL(*this, OnPause());
  SendEosNotification(1);
}

}  // namespace media
