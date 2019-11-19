// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/text_renderer.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/text_cue.h"

namespace media {

TextRenderer::TextRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const AddTextTrackCB& add_text_track_cb)
    : task_runner_(task_runner),
      add_text_track_cb_(add_text_track_cb),
      state_(kUninitialized),
      pending_read_count_(0) {}

TextRenderer::~TextRenderer() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  text_track_state_map_.clear();
  if (pause_cb_)
    std::move(pause_cb_).Run();
}

void TextRenderer::Initialize(const base::Closure& ended_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(ended_cb);
  DCHECK_EQ(kUninitialized, state_)  << "state_ " << state_;
  DCHECK(text_track_state_map_.empty());
  DCHECK_EQ(pending_read_count_, 0);
  DCHECK(pending_eos_set_.empty());
  DCHECK(!ended_cb_);

  ended_cb_ = ended_cb;
  state_ = kPaused;
}

void TextRenderer::StartPlaying() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPaused) << "state_ " << state_;

  for (auto itr = text_track_state_map_.begin();
       itr != text_track_state_map_.end(); ++itr) {
    TextTrackState* state = itr->second.get();
    if (state->read_state == TextTrackState::kReadPending) {
      DCHECK_GT(pending_read_count_, 0);
      continue;
    }

    Read(state, itr->first);
  }

  state_ = kPlaying;
}

void TextRenderer::Pause(const base::Closure& callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kPlaying || state_ == kEnded) << "state_ " << state_;
  DCHECK_GE(pending_read_count_, 0);

  if (pending_read_count_ == 0) {
    state_ = kPaused;
    task_runner_->PostTask(FROM_HERE, callback);
    return;
  }

  pause_cb_ = callback;
  state_ = kPausePending;
}

void TextRenderer::Flush(const base::Closure& callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(pending_read_count_, 0);
  DCHECK(state_ == kPaused) << "state_ " << state_;

  for (auto itr = text_track_state_map_.begin();
       itr != text_track_state_map_.end(); ++itr) {
    pending_eos_set_.insert(itr->first);
    itr->second->text_ranges_.Reset();
  }
  DCHECK_EQ(pending_eos_set_.size(), text_track_state_map_.size());
  task_runner_->PostTask(FROM_HERE, callback);
}

void TextRenderer::AddTextStream(DemuxerStream* text_stream,
                                 const TextTrackConfig& config) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ != kUninitialized) << "state_ " << state_;
  DCHECK(text_track_state_map_.find(text_stream) ==
         text_track_state_map_.end());
  DCHECK(pending_eos_set_.find(text_stream) ==
         pending_eos_set_.end());

  AddTextTrackDoneCB done_cb =
      BindToCurrentLoop(base::Bind(&TextRenderer::OnAddTextTrackDone,
                                   weak_factory_.GetWeakPtr(),
                                   text_stream));

  add_text_track_cb_.Run(config, done_cb);
}

void TextRenderer::RemoveTextStream(DemuxerStream* text_stream) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  auto itr = text_track_state_map_.find(text_stream);
  DCHECK(itr != text_track_state_map_.end());

  TextTrackState* state = itr->second.get();
  DCHECK_EQ(state->read_state, TextTrackState::kReadIdle);
  text_track_state_map_.erase(itr);

  pending_eos_set_.erase(text_stream);
}

bool TextRenderer::HasTracks() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return !text_track_state_map_.empty();
}

void TextRenderer::BufferReady(DemuxerStream* stream,
                               DemuxerStream::Status status,
                               scoped_refptr<DecoderBuffer> input) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(status, DemuxerStream::kConfigChanged);

  if (status == DemuxerStream::kAborted) {
    DCHECK(!input);
    DCHECK_GT(pending_read_count_, 0);
    DCHECK(pending_eos_set_.find(stream) != pending_eos_set_.end());

    auto itr = text_track_state_map_.find(stream);
    DCHECK(itr != text_track_state_map_.end());

    TextTrackState* state = itr->second.get();
    DCHECK_EQ(state->read_state, TextTrackState::kReadPending);

    --pending_read_count_;
    state->read_state = TextTrackState::kReadIdle;

    switch (state_) {
      case kPlaying:
        return;

      case kPausePending:
        if (pending_read_count_ == 0) {
          state_ = kPaused;
          std::move(pause_cb_).Run();
        }

        return;

      case kPaused:
      case kUninitialized:
      case kEnded:
        NOTREACHED();
        return;
    }

    NOTREACHED();
    return;
  }

  if (input->end_of_stream()) {
    CueReady(stream, nullptr);
    return;
  }

  DCHECK_EQ(status, DemuxerStream::kOk);
  DCHECK_GE(input->side_data_size(), 2u);

  // The side data contains both the cue id and cue settings,
  // each terminated with a NUL.
  const char* id_ptr = reinterpret_cast<const char*>(input->side_data());
  size_t id_len = strlen(id_ptr);
  std::string id(id_ptr, id_len);

  const char* settings_ptr = id_ptr + id_len + 1;
  size_t settings_len = strlen(settings_ptr);
  std::string settings(settings_ptr, settings_len);

  // The cue payload is stored in the data-part of the input buffer.
  std::string text(input->data(), input->data() + input->data_size());

  scoped_refptr<TextCue> text_cue(
      new TextCue(input->timestamp(),
                  input->duration(),
                  id,
                  settings,
                  text));

  CueReady(stream, text_cue);
}

void TextRenderer::CueReady(
    DemuxerStream* text_stream,
    const scoped_refptr<TextCue>& text_cue) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(state_, kUninitialized);
  DCHECK_GT(pending_read_count_, 0);
  DCHECK(pending_eos_set_.find(text_stream) != pending_eos_set_.end());

  auto itr = text_track_state_map_.find(text_stream);
  DCHECK(itr != text_track_state_map_.end());

  TextTrackState* state = itr->second.get();
  DCHECK_EQ(state->read_state, TextTrackState::kReadPending);
  DCHECK(state->text_track);

  --pending_read_count_;
  state->read_state = TextTrackState::kReadIdle;

  switch (state_) {
    case kPlaying: {
      if (text_cue.get())
        break;

      const size_t count = pending_eos_set_.erase(text_stream);
      DCHECK_EQ(count, 1U);

      if (pending_eos_set_.empty()) {
        DCHECK_EQ(pending_read_count_, 0);
        state_ = kEnded;
        task_runner_->PostTask(FROM_HERE, ended_cb_);
        return;
      }

      DCHECK_GT(pending_read_count_, 0);
      return;
    }
    case kPausePending: {
      if (text_cue.get())
        break;

      const size_t count = pending_eos_set_.erase(text_stream);
      DCHECK_EQ(count, 1U);

      if (pending_read_count_ > 0) {
        DCHECK(!pending_eos_set_.empty());
        return;
      }

      state_ = kPaused;
      std::move(pause_cb_).Run();

      return;
    }

    case kPaused:
    case kUninitialized:
    case kEnded:
      NOTREACHED();
      return;
  }

  base::TimeDelta start = text_cue->timestamp();

  if (state->text_ranges_.AddCue(start)) {
    base::TimeDelta end = start + text_cue->duration();

    state->text_track->addWebVTTCue(start, end,
                                    text_cue->id(),
                                    text_cue->text(),
                                    text_cue->settings());
  }

  if (state_ == kPlaying) {
    Read(state, text_stream);
    return;
  }

  if (pending_read_count_ == 0) {
      DCHECK_EQ(state_, kPausePending) << "state_ " << state_;
      state_ = kPaused;
      std::move(pause_cb_).Run();
  }
}

void TextRenderer::OnAddTextTrackDone(DemuxerStream* text_stream,
                                      std::unique_ptr<TextTrack> text_track) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(state_, kUninitialized);
  DCHECK(text_stream);
  DCHECK(text_track);

  std::unique_ptr<TextTrackState> state(
      new TextTrackState(std::move(text_track)));
  text_track_state_map_[text_stream] = std::move(state);
  pending_eos_set_.insert(text_stream);

  if (state_ == kPlaying)
    Read(text_track_state_map_[text_stream].get(), text_stream);
}

void TextRenderer::Read(
    TextTrackState* state,
    DemuxerStream* text_stream) {
  DCHECK_NE(state->read_state, TextTrackState::kReadPending);

  state->read_state = TextTrackState::kReadPending;
  ++pending_read_count_;

  text_stream->Read(base::BindOnce(&TextRenderer::BufferReady,
                                   weak_factory_.GetWeakPtr(), text_stream));
}

TextRenderer::TextTrackState::TextTrackState(std::unique_ptr<TextTrack> tt)
    : read_state(kReadIdle), text_track(std::move(tt)) {}

TextRenderer::TextTrackState::~TextTrackState() = default;

}  // namespace media
