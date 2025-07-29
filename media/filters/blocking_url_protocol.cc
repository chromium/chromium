// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/blocking_url_protocol.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "media/base/data_source.h"
#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

BlockingUrlProtocol::BlockingUrlProtocol(DataSource* data_source,
                                         const base::RepeatingClosure& error_cb)
    : data_source_(data_source),
      error_cb_(error_cb),
      is_streaming_(data_source_->IsStreaming()),
      aborted_(base::WaitableEvent::ResetPolicy::MANUAL,
               base::WaitableEvent::InitialState::NOT_SIGNALED),  // We never
                                                                  // want to
                                                                  // reset
                                                                  // |aborted_|.
      read_complete_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                     base::WaitableEvent::InitialState::NOT_SIGNALED),
      last_read_bytes_(0),
      read_position_(0) {}

BlockingUrlProtocol::~BlockingUrlProtocol() = default;

void BlockingUrlProtocol::Abort() {
  aborted_.Signal();
  base::AutoLock lock(data_source_lock_);
  data_source_ = nullptr;
}

int BlockingUrlProtocol::Read(int size, uint8_t* data) {
  {
    // Read errors are unrecoverable.
    base::AutoLock lock(data_source_lock_);
    if (!data_source_) {
      DCHECK(aborted_.IsSignaled());
      return AVERROR(EIO);
    }

    // Not sure this can happen, but it's unclear from the ffmpeg code, so guard
    // against it.
    if (size < 0)
      return AVERROR(EIO);
    if (!size)
      return 0;

    int64_t file_size;
    if (data_source_->GetSize(&file_size) && read_position_ >= file_size)
      return AVERROR_EOF;

    // Blocking read from data source until either:
    //   1) |last_read_bytes_| is set and |read_complete_| is signalled
    //   2) |aborted_| is signalled
    data_source_->Read(read_position_, size, data,
                       base::BindOnce(&BlockingUrlProtocol::SignalReadCompleted,
                                      base::Unretained(this)));
  }

  base::WaitableEvent* events[] = { &aborted_, &read_complete_ };
  size_t index;
  {
    base::ScopedAllowBaseSyncPrimitives allow_base_sync_primitives;
    index = base::WaitableEvent::WaitMany(events, std::size(events));
  }

  if (events[index] == &aborted_)
    return AVERROR(EIO);

  if (last_read_bytes_ == DataSource::kReadError) {
    aborted_.Signal();
    error_cb_.Run();
    return AVERROR(EIO);
  }

  if (last_read_bytes_ == DataSource::kAborted)
    return AVERROR(EIO);

  read_position_ += last_read_bytes_;
  return last_read_bytes_;
}

bool BlockingUrlProtocol::GetPosition(int64_t* position_out) {
  *position_out = read_position_;
  return true;
}

bool BlockingUrlProtocol::SetPosition(int64_t position) {
  base::AutoLock lock(data_source_lock_);
  int64_t file_size;
  if (!data_source_ ||
      (data_source_->GetSize(&file_size) && position > file_size) ||
      position < 0) {
    return false;
  }

  read_position_ = position;
  return true;
}

bool BlockingUrlProtocol::GetSize(int64_t* size_out) {
  base::AutoLock lock(data_source_lock_);
  return data_source_ ? data_source_->GetSize(size_out) : false;
}

bool BlockingUrlProtocol::IsStreaming() {
  return is_streaming_;
}

void BlockingUrlProtocol::SignalReadCompleted(int size) {
  last_read_bytes_ = size;
  read_complete_.Signal();
}

}  // namespace media
