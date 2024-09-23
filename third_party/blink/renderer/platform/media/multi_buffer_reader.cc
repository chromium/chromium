// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/media/multi_buffer_reader.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/net_errors.h"

namespace blink {

MultiBufferReader::MultiBufferReader(
    MultiBuffer* multibuffer,
    int64_t start,
    int64_t end,
    bool is_client_audio_element,
    base::RepeatingCallback<void(int64_t, int64_t)> progress_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : multibuffer_(multibuffer),
      // If end is -1, we use a very large (but still supported) value instead.
      end_(end == -1LL ? (1LL << (multibuffer->block_size_shift() + 30)) : end),
      preload_high_(0),
      preload_low_(0),
      max_buffer_forward_(0),
      max_buffer_backward_(0),
      current_buffer_size_(0),
      pinned_range_(0, 0),
      pos_(start),
      is_client_audio_element_(is_client_audio_element),
      preload_pos_(-1),
      loading_(true),
      current_wait_size_(0),
      progress_callback_(std::move(progress_callback)),
      task_runner_(std::move(task_runner)) {
  DCHECK_GE(start, 0);
  DCHECK_GE(end_, 0);
}

MultiBufferReader::~MultiBufferReader() {
  PinRange(0, 0);
  multibuffer_->RemoveReader(preload_pos_, this);
  multibuffer_->IncrementMaxSize(-current_buffer_size_);
  multibuffer_->CleanupWriters(preload_pos_);
}

void MultiBufferReader::Seek(int64_t pos) {
  DCHECK_GE(pos, 0);
  if (pos == pos_)
    return;
  PinRange(block(pos - max_buffer_backward_),
           block_ceil(pos + max_buffer_forward_));

  multibuffer_->RemoveReader(preload_pos_, this);
  MultiBufferBlockId old_preload_pos = preload_pos_;
  preload_pos_ = block(pos);
  pos_ = pos;
  UpdateInternalState();
  multibuffer_->CleanupWriters(old_preload_pos);
}

void MultiBufferReader::SetMaxBuffer(int64_t buffer_size) {
  // Safe, because we know this doesn't actually prune the cache right away.
  int64_t new_buffer_size = block_ceil(buffer_size);
  multibuffer_->IncrementMaxSize(new_buffer_size - current_buffer_size_);
  current_buffer_size_ = new_buffer_size;
}

void MultiBufferReader::SetPinRange(int64_t backward, int64_t forward) {
  // Safe, because we know this doesn't actually prune the cache right away.
  max_buffer_backward_ = backward;
  max_buffer_forward_ = forward;
  PinRange(block(pos_ - max_buffer_backward_),
           block_ceil(pos_ + max_buffer_forward_));
}

int64_t MultiBufferReader::AvailableAt(int64_t pos) const {
  int64_t unavailable_byte_pos =
      static_cast<int64_t>(multibuffer_->FindNextUnavailable(block(pos)))
      << multibuffer_->block_size_shift();
  return std::max<int64_t>(0, unavailable_byte_pos - pos);
}

int64_t MultiBufferReader::TryReadAt(int64_t pos, uint8_t* data, int64_t len) {
  DCHECK_GT(len, 0);
  std::vector<scoped_refptr<media::DataBuffer>> buffers;
  multibuffer_->GetBlocksThreadsafe(block(pos), block_ceil(pos + len),
                                    &buffers);
  int64_t bytes_read = 0;
  for (auto& buffer : buffers) {
    if (buffer->end_of_stream())
      break;
    int64_t offset = pos & ((1LL << multibuffer_->block_size_shift()) - 1);
    if (offset > static_cast<int64_t>(buffer->data_size()))
      break;
    int64_t tocopy = std::min(len - bytes_read, buffer->data_size() - offset);
    memcpy(data, buffer->data() + offset, static_cast<size_t>(tocopy));
    data += tocopy;
    bytes_read += tocopy;
    if (bytes_read == len)
      break;
    if (block(pos + tocopy) != block(pos) + 1)
      break;
    pos += tocopy;
  }
  return bytes_read;
}

int64_t MultiBufferReader::TryRead(uint8_t* data, int64_t len) {
  int64_t bytes_read = TryReadAt(pos_, data, len);
  Seek(pos_ + bytes_read);
  return bytes_read;
}

int MultiBufferReader::Wait(int64_t len, base::OnceClosure cb) {
  DCHECK_LE(pos_ + len, end_);
  DCHECK_NE(Available(), -1);
  DCHECK_LE(len, max_buffer_forward_);
  current_wait_size_ = len;

  cb_.Reset();
  UpdateInternalState();

  if (Available() >= current_wait_size_) {
    return net::OK;
  } else {
    cb_ = std::move(cb);
    return net::ERR_IO_PENDING;
  }
}

void MultiBufferReader::SetPreload(int64_t preload_high, int64_t preload_low) {
  DCHECK_GE(preload_high, preload_low);
  multibuffer_->RemoveReader(preload_pos_, this);
  preload_pos_ = block(pos_);
  preload_high_ = preload_high;
  preload_low_ = preload_low;
  UpdateInternalState();
}

bool MultiBufferReader::IsLoading() const {
  return loading_;
}

void MultiBufferReader::CheckWait() {
  if (!cb_.is_null() &&
      (Available() >= current_wait_size_ || Available() == -1)) {
    // We redirect the call through a weak pointer to ourselves to guarantee
    // there are no callbacks from us after we've been destroyed.
    current_wait_size_ = 0;
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MultiBufferReader::Call,
                                  weak_factory_.GetWeakPtr(), std::move(cb_)));
  }
}

void MultiBufferReader::Call(base::OnceClosure cb) const {
  std::move(cb).Run();
}

void MultiBufferReader::UpdateEnd(MultiBufferBlockId p) {
  auto i = multibuffer_->map().find(p - 1);
  if (i != multibuffer_->map().end() && i->second->end_of_stream()) {
    // This is an upper limit because the last-to-one block is allowed
    // to be smaller than the rest of the blocks.
    int64_t size_upper_limit = static_cast<int64_t>(p)
                               << multibuffer_->block_size_shift();
    end_ = std::min(end_, size_upper_limit);
  }
}

void MultiBufferReader::NotifyAvailableRange(
    const Interval<MultiBufferBlockId>& range) {
  // Update end_ if we can.
  if (range.end > range.begin) {
    UpdateEnd(range.end);
  }
  UpdateInternalState();
  if (!progress_callback_.is_null()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(progress_callback_,
                       static_cast<int64_t>(range.begin)
                           << multibuffer_->block_size_shift(),
                       (static_cast<int64_t>(range.end)
                        << multibuffer_->block_size_shift()) +
                           multibuffer_->UncommittedBytesAt(range.end)));
  }
}

void MultiBufferReader::UpdateInternalState() {
  int64_t effective_preload = loading_ ? preload_high_ : preload_low_;

  loading_ = false;
  if (preload_pos_ == -1) {
    preload_pos_ = block(pos_);
    DCHECK_GE(preload_pos_, 0);
  }

  // Note that we might not have been added to the multibuffer,
  // removing ourselves is a no-op in that case.
  multibuffer_->RemoveReader(preload_pos_, this);

  // We explicitly allow preloading to go beyond the pinned region in the cache.
  // It only happens when we want to preload something into the disk cache.
  // Thus it is possible to have blocks between our current reading position
  // and preload_pos_ be unavailable. When we get a Seek() call (possibly
  // through TryRead()) we reset the preload_pos_ to the current reading
  // position, and preload_pos_ will become the first unavailable block after
  // our current reading position again.
  preload_pos_ = multibuffer_->FindNextUnavailable(preload_pos_);
  UpdateEnd(preload_pos_);
  DCHECK_GE(preload_pos_, 0);

  MultiBuffer::BlockId max_preload = block_ceil(
      std::min(end_, pos_ + std::max(effective_preload, current_wait_size_)));

  DVLOG(3) << "UpdateInternalState"
           << " pp = " << preload_pos_
           << " block_ceil(end_) = " << block_ceil(end_) << " end_ = " << end_
           << " max_preload " << max_preload;

  multibuffer_->SetIsClientAudioElement(is_client_audio_element_);
  if (preload_pos_ < block_ceil(end_)) {
    if (preload_pos_ < max_preload) {
      loading_ = true;
      multibuffer_->AddReader(preload_pos_, this);
    } else if (multibuffer_->Contains(preload_pos_ - 1)) {
      --preload_pos_;
      multibuffer_->AddReader(preload_pos_, this);
    }
  }
  CheckWait();
}

void MultiBufferReader::PinRange(MultiBuffer::BlockId begin,
                                 MultiBuffer::BlockId end) {
  // Use a rangemap to compute the diff in pinning.
  IntervalMap<MultiBuffer::BlockId, int32_t> tmp;
  tmp.IncrementInterval(pinned_range_.begin, pinned_range_.end, -1);
  tmp.IncrementInterval(begin, end, 1);
  multibuffer_->PinRanges(tmp);
  pinned_range_.begin = begin;
  pinned_range_.end = end;
}

}  // namespace blink
