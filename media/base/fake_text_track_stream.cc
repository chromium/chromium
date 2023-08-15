// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/fake_text_track_stream.h"

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/decoder_buffer.h"
#include "media/base/webvtt_util.h"

namespace media {

FakeTextTrackStream::FakeTextTrackStream()
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      stopping_(false) {}

FakeTextTrackStream::~FakeTextTrackStream() {
  DCHECK(!read_cb_);
}

// Only return one buffer at a time so we ignore the count.
void FakeTextTrackStream::Read(uint32_t /*count*/, ReadCB read_cb) {
  DCHECK(read_cb);
  DCHECK(!read_cb_);
  OnRead();
  read_cb_ = std::move(read_cb);

  if (stopping_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&FakeTextTrackStream::AbortPendingRead,
                                  base::Unretained(this)));
  }
}

DemuxerStream::Type FakeTextTrackStream::type() const {
  return DemuxerStream::TEXT;
}

bool FakeTextTrackStream::SupportsConfigChanges() { return false; }

void FakeTextTrackStream::SatisfyPendingRead(
    const base::TimeDelta& start,
    const base::TimeDelta& duration,
    const std::string& id,
    const std::string& content,
    const std::string& settings) {
  DCHECK(read_cb_);

  const uint8_t* const data_buf =
      reinterpret_cast<const uint8_t*>(content.data());
  const int data_len = static_cast<int>(content.size());

  // TODO(crbug.com/1471504): This is now broken without side data; remove.
  scoped_refptr<DecoderBuffer> buffer =
      DecoderBuffer::CopyFrom(data_buf, data_len);

  buffer->set_timestamp(start);
  buffer->set_duration(duration);

  // Assume all fake text buffers are keyframes.
  buffer->set_is_key_frame(true);

  std::move(read_cb_).Run(kOk, {std::move(buffer)});
}

void FakeTextTrackStream::AbortPendingRead() {
  DCHECK(read_cb_);
  std::move(read_cb_).Run(kAborted, {});
}

void FakeTextTrackStream::SendEosNotification() {
  DCHECK(read_cb_);
  std::move(read_cb_).Run(kOk, {DecoderBuffer::CreateEOSBuffer()});
}

void FakeTextTrackStream::Stop() {
  stopping_ = true;
  if (read_cb_)
    AbortPendingRead();
}

}  // namespace media
