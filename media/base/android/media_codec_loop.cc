// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_loop.h"

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/timestamp_constants.h"

namespace media {
namespace {

constexpr base::TimeDelta kDecodePollDelay =
    base::TimeDelta::FromMilliseconds(10);
constexpr base::TimeDelta kNoWaitTimeout = base::TimeDelta::FromMicroseconds(0);
constexpr base::TimeDelta kIdleTimerTimeout = base::TimeDelta::FromSeconds(1);

}  // namespace

MediaCodecLoop::InputData::InputData() {}

MediaCodecLoop::InputData::InputData(const InputData& other)
    : memory(other.memory),
      length(other.length),
      key_id(other.key_id),
      iv(other.iv),
      subsamples(other.subsamples),
      presentation_time(other.presentation_time),
      is_eos(other.is_eos),
      encryption_scheme(other.encryption_scheme) {}

MediaCodecLoop::InputData::~InputData() {}

MediaCodecLoop::MediaCodecLoop(
    int sdk_int,
    Client* client,
    std::unique_ptr<MediaCodecBridge> media_codec,
    scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner,
    bool disable_timer)
    : state_(STATE_READY),
      client_(client),
      media_codec_(std::move(media_codec)),
      pending_input_buf_index_(kInvalidBufferIndex),
      sdk_int_(sdk_int),
      disable_timer_(disable_timer) {
  if (timer_task_runner)
    io_timer_.SetTaskRunner(timer_task_runner);
  // TODO(liberato): should this DCHECK?
  if (media_codec_ == nullptr)
    SetState(STATE_ERROR);
}

MediaCodecLoop::~MediaCodecLoop() {
  io_timer_.Stop();
}

void MediaCodecLoop::SetTestTickClock(const base::TickClock* test_tick_clock) {
  test_tick_clock_ = test_tick_clock;
}

void MediaCodecLoop::OnKeyAdded() {
  if (state_ == STATE_WAITING_FOR_KEY)
    SetState(STATE_READY);

  ExpectWork();
}

bool MediaCodecLoop::TryFlush() {
  // We do not clear the input queue here.  It depends on the caller.
  // For decoder reset, then it is appropriate.  Otherwise, the requests might
  // simply be sent to us later, such as on a format change.

  // STATE_DRAINED seems like it allows flush, but it causes test failures.
  // crbug.com/624878
  if (state_ == STATE_ERROR || state_ == STATE_DRAINED)
    return false;

  // Actually try to flush!
  io_timer_.Stop();

  if (media_codec_->Flush() != MEDIA_CODEC_OK) {
    // TODO(liberato): we might not want to notify the client about this.
    SetState(STATE_ERROR);
    return false;
  }

  SetState(STATE_READY);
  return true;
}

void MediaCodecLoop::ExpectWork() {
  // Start / reset the timer, since we believe that progress can be made soon,
  // even if not immediately.
  ManageTimer(true);
  DoPendingWork();
}

void MediaCodecLoop::DoPendingWork() {
  if (state_ == STATE_ERROR)
    return;

  bool did_work = false, did_input = false, did_output = false;
  do {
    did_input = ProcessOneInputBuffer();
    did_output = ProcessOneOutputBuffer();
    if (did_input || did_output)
      did_work = true;
  } while (did_input || did_output);

  // TODO(liberato): add "start_timer" for AVDA.
  ManageTimer(did_work);
}

bool MediaCodecLoop::ProcessOneInputBuffer() {
  if (state_ != STATE_READY)
    return false;

  // We can only queue a buffer if there is input from the client, or if we
  // tried previously but had to wait for a key.  In the latter case, MediaCodec
  // already has the data.
  if (pending_input_buf_index_ == kInvalidBufferIndex &&
      !client_->IsAnyInputPending()) {
    return false;
  }

  // DequeueInputBuffer() may set STATE_ERROR.
  InputBuffer input_buffer = DequeueInputBuffer();

  if (input_buffer.index == kInvalidBufferIndex)
    return false;

  // EnqueueInputBuffer() may change the state.
  EnqueueInputBuffer(input_buffer);
  return state_ != STATE_ERROR;
}

MediaCodecLoop::InputBuffer MediaCodecLoop::DequeueInputBuffer() {
  DVLOG(2) << __func__;

  // Do not dequeue a new input buffer if we failed with MEDIA_CODEC_NO_KEY.
  // That status does not return the input buffer back to the pool of
  // available input buffers. We have to reuse it later when calling
  // MediaCodec's QueueSecureInputBuffer().
  if (pending_input_buf_index_ != kInvalidBufferIndex) {
    InputBuffer result(pending_input_buf_index_, true);
    pending_input_buf_index_ = kInvalidBufferIndex;
    return result;
  }

  int input_buf_index = kInvalidBufferIndex;

  media::MediaCodecStatus status =
      media_codec_->DequeueInputBuffer(kNoWaitTimeout, &input_buf_index);
  switch (status) {
    case media::MEDIA_CODEC_TRY_AGAIN_LATER:
      break;

    case media::MEDIA_CODEC_ERROR:
      DLOG(ERROR) << __func__ << ": MEDIA_CODEC_ERROR from DequeInputBuffer";
      SetState(STATE_ERROR);
      break;

    case media::MEDIA_CODEC_OK:
      break;

    default:
      NOTREACHED() << "Unknown DequeueInputBuffer status " << status;
      SetState(STATE_ERROR);
      break;
  }

  return InputBuffer(input_buf_index, false);
}

void MediaCodecLoop::EnqueueInputBuffer(const InputBuffer& input_buffer) {
  DCHECK_NE(input_buffer.index, kInvalidBufferIndex);

  InputData input_data;
  if (input_buffer.is_pending) {
    // A pending buffer is already filled with data, no need to copy it again.
    input_data = pending_input_buf_data_;
  } else {
    input_data = client_->ProvideInputData();
  }

  if (input_data.is_eos) {
    media_codec_->QueueEOS(input_buffer.index);
    SetState(STATE_DRAINING);
    client_->OnInputDataQueued(true);
    return;
  }

  media::MediaCodecStatus status = MEDIA_CODEC_OK;

  if (input_data.encryption_scheme != EncryptionScheme::kUnencrypted) {
    // Note that input_data might not have a valid memory ptr if this is a
    // re-send of a buffer that was sent before decryption keys arrived.

    status = media_codec_->QueueSecureInputBuffer(
        input_buffer.index, input_data.memory, input_data.length,
        input_data.key_id, input_data.iv, input_data.subsamples,
        input_data.encryption_scheme, input_data.encryption_pattern,
        input_data.presentation_time);

  } else {
    status = media_codec_->QueueInputBuffer(
        input_buffer.index, input_data.memory, input_data.length,
        input_data.presentation_time);
  }

  switch (status) {
    case MEDIA_CODEC_ERROR:
      DLOG(ERROR) << __func__ << ": MEDIA_CODEC_ERROR from QueueInputBuffer";
      client_->OnInputDataQueued(false);
      // Transition to the error state after running the completion cb, to keep
      // it in order if the client chooses to flush its queue.
      SetState(STATE_ERROR);
      break;

    case MEDIA_CODEC_NO_KEY:
      // Do not call the completion cb here.  It will be called when we retry
      // after getting the key.
      pending_input_buf_index_ = input_buffer.index;
      pending_input_buf_data_ = input_data;
      // MediaCodec has a copy of the data already.  When we call again, be sure
      // to send in nullptr for the source.  Note that the client doesn't
      // guarantee that the pointer will remain valid after we return anyway.
      pending_input_buf_data_.memory = nullptr;
      client_->OnWaiting(WaitingReason::kNoDecryptionKey);
      SetState(STATE_WAITING_FOR_KEY);
      // Do not call OnInputDataQueued yet.
      break;

    case MEDIA_CODEC_OK:
      client_->OnInputDataQueued(true);
      break;

    default:
      NOTREACHED() << "Unknown Queue(Secure)InputBuffer status " << status;
      client_->OnInputDataQueued(false);
      SetState(STATE_ERROR);
      break;
  }
}

bool MediaCodecLoop::ProcessOneOutputBuffer() {
  // TODO(liberato): When merging AVDA, we will also have to ask the client if
  // it can accept another output buffer.

  if (state_ == STATE_ERROR)
    return false;

  OutputBuffer out;
  MediaCodecStatus status = media_codec_->DequeueOutputBuffer(
      kNoWaitTimeout, &out.index, &out.offset, &out.size, &out.pts, &out.is_eos,
      &out.is_key_frame);

  bool did_work = false;
  switch (status) {
    case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
      // Output buffers are replaced in MediaCodecBridge, nothing to do.
      did_work = true;
      break;

    case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED:
      if (!client_->OnOutputFormatChanged())
        SetState(STATE_ERROR);
      did_work = state_ != STATE_ERROR;
      break;

    case MEDIA_CODEC_OK:
      // We got the decoded frame or EOS.
      if (out.is_eos) {
        // Set state STATE_DRAINED after we have received EOS frame at the
        // output. media_decoder_job.cc says: once output EOS has occurred, we
        // should not be asked to decode again.
        DCHECK_EQ(state_, STATE_DRAINING);
        SetState(STATE_DRAINED);

        DCHECK_NE(out.index, kInvalidBufferIndex);
        DCHECK(media_codec_);

        media_codec_->ReleaseOutputBuffer(out.index, false);

        if (!client_->OnDecodedEos(out))
          SetState(STATE_ERROR);
      } else {
        if (!client_->OnDecodedFrame(out))
          SetState(STATE_ERROR);
      }

      did_work = true;
      break;

    case MEDIA_CODEC_TRY_AGAIN_LATER:
      // Nothing to do.
      break;

    case MEDIA_CODEC_ERROR:
      DLOG(ERROR) << __func__ << ": MEDIA_CODEC_ERROR from DequeueOutputBuffer";
      SetState(STATE_ERROR);
      break;

    default:
      NOTREACHED() << "Unknown DequeueOutputBuffer status " << status;
      SetState(STATE_ERROR);
      break;
  }

  return did_work;
}

void MediaCodecLoop::ManageTimer(bool did_work) {
  if (disable_timer_)
    return;

  bool should_be_running = true;

  // One might also use DefaultTickClock, but then ownership becomes harder.
  base::TimeTicks now = (test_tick_clock_ ? test_tick_clock_->NowTicks()
                                          : base::TimeTicks::Now());
  if (did_work || idle_time_begin_ == base::TimeTicks()) {
    idle_time_begin_ = now;
  } else {
    // Make sure that we have done work recently enough, else stop the timer.
    if (now - idle_time_begin_ > kIdleTimerTimeout)
      should_be_running = false;
  }

  if (should_be_running && !io_timer_.IsRunning()) {
    io_timer_.Start(FROM_HERE, kDecodePollDelay, this,
                    &MediaCodecLoop::DoPendingWork);
  } else if (!should_be_running && io_timer_.IsRunning()) {
    io_timer_.Stop();
  }
}

void MediaCodecLoop::SetState(State new_state) {
  const State old_state = state_;
  state_ = new_state;
  if (old_state != new_state && new_state == STATE_ERROR)
    client_->OnCodecLoopError();
}

MediaCodecBridge* MediaCodecLoop::GetCodec() const {
  return media_codec_.get();
}

// static
const char* MediaCodecLoop::AsString(State state) {
#define RETURN_STRING(x) \
  case x:                \
    return #x;

  switch (state) {
    RETURN_STRING(STATE_READY);
    RETURN_STRING(STATE_WAITING_FOR_KEY);
    RETURN_STRING(STATE_DRAINING);
    RETURN_STRING(STATE_DRAINED);
    RETURN_STRING(STATE_ERROR);
  }
#undef RETURN_STRING

  NOTREACHED() << "Unknown state " << state;
  return nullptr;
}

}  // namespace media
