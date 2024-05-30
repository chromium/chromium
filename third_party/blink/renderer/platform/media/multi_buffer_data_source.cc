// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/multi_buffer_data_source.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/media_log.h"
#include "net/base/net_errors.h"
#include "third_party/blink/renderer/platform/media/buffered_data_source_host_impl.h"
#include "third_party/blink/renderer/platform/media/multi_buffer_reader.h"
#include "url/gurl.h"

namespace blink {
namespace {

// Minimum preload buffer.
const int64_t kMinBufferPreload = 2 << 20;  // 2 Mb
// Maxmimum preload buffer.
const int64_t kMaxBufferPreload = 50 << 20;  // 50 Mb

// If preload_ == METADATA, preloading size will be
// shifted down this many bits. This shift turns
// one Mb into one 32k block.
// This seems to be the smallest amount of preload we can do without
// ending up repeatedly closing and re-opening the connection
// due to read calls after OnBufferingHaveEnough have been called.
const int64_t kMetadataShift = 6;

// Preload this much extra, then stop preloading until we fall below the
// preload_seconds_.value().
const int64_t kPreloadHighExtra = 1 << 20;  // 1 Mb

// Default pin region size.
// Note that we go over this if preload is calculated high enough.
const int64_t kDefaultPinSize = 25 << 20;  // 25 Mb

// If bitrate is not known, use this.
const int64_t kDefaultBitrate = 200 * 8 << 10;  // 200 Kbps.

// Maximum bitrate for buffer calculations.
const int64_t kMaxBitrate = 20 * 8 << 20;  // 20 Mbps.

// Maximum playback rate for buffer calculations.
const double kMaxPlaybackRate = 25.0;

// Extra buffer accumulation speed, in terms of download buffer.
const int kSlowPreloadPercentage = 10;

// Update buffer sizes every 32 progress updates.
const int kUpdateBufferSizeFrequency = 32;

// How long to we delay a seek after a read?
constexpr base::TimeDelta kSeekDelay = base::Milliseconds(20);

}  // namespace

class MultiBufferDataSource::ReadOperation {
 public:
  ReadOperation() = delete;
  ReadOperation(int64_t position,
                int size,
                uint8_t* data,
                media::DataSource::ReadCB callback);
  ReadOperation(const ReadOperation&) = delete;
  ReadOperation& operator=(const ReadOperation&) = delete;
  ~ReadOperation();

  // Runs |callback_| with the given |result|, deleting the operation
  // afterwards.
  static void Run(std::unique_ptr<ReadOperation> read_op, int result);

  int64_t position() { return position_; }
  int size() { return size_; }
  uint8_t* data() { return data_; }

 private:
  const int64_t position_;
  const int size_;
  raw_ptr<uint8_t, DanglingUntriaged> data_;
  media::DataSource::ReadCB callback_;
};

MultiBufferDataSource::ReadOperation::ReadOperation(
    int64_t position,
    int size,
    uint8_t* data,
    media::DataSource::ReadCB callback)
    : position_(position),
      size_(size),
      data_(data),
      callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());
}

MultiBufferDataSource::ReadOperation::~ReadOperation() {
  DCHECK(callback_.is_null());
}

// static
void MultiBufferDataSource::ReadOperation::Run(
    std::unique_ptr<ReadOperation> read_op,
    int result) {
  std::move(read_op->callback_).Run(result);
}

MultiBufferDataSource::MultiBufferDataSource(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    scoped_refptr<UrlData> url_data_arg,
    media::MediaLog* media_log,
    BufferedDataSourceHost* host,
    DownloadingCB downloading_cb)
    : total_bytes_(kPositionNotSpecified),
      streaming_(false),
      loading_(false),
      failed_(false),
      render_task_runner_(task_runner),
      url_data_(std::move(url_data_arg)),
      stop_signal_received_(false),
      media_has_played_(false),
      single_origin_(true),
      cancel_on_defer_(false),
      preload_(AUTO),
      bitrate_(0),
      playback_rate_(0.0),
      media_log_(media_log->Clone()),
      host_(host),
      downloading_cb_(std::move(downloading_cb)) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
  DCHECK(host_);
  DCHECK(downloading_cb_);
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  DCHECK(url_data_.get());
  url_data_->Use();
  url_data_->OnRedirect(
      base::BindOnce(&MultiBufferDataSource::OnRedirected, weak_ptr_));
}

MultiBufferDataSource::~MultiBufferDataSource() {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
}

bool MultiBufferDataSource::media_has_played() const {
  return media_has_played_;
}

bool MultiBufferDataSource::AssumeFullyBuffered() const {
  DCHECK(url_data_);
  return !url_data_->url().SchemeIsHTTPOrHTTPS();
}

void MultiBufferDataSource::SetReader(
    std::unique_ptr<MultiBufferReader> reader) {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  reader_ = std::move(reader);
}

void MultiBufferDataSource::CreateResourceLoader(int64_t first_byte_position,
                                                 int64_t last_byte_position) {
  DCHECK(render_task_runner_->BelongsToCurrentThread());

  SetReader(std::make_unique<MultiBufferReader>(
      url_data_->multibuffer(), first_byte_position, last_byte_position,
      is_client_audio_element_,
      base::BindRepeating(&MultiBufferDataSource::ProgressCallback, weak_ptr_),
      render_task_runner_));
  UpdateBufferSizes();
}

void MultiBufferDataSource::CreateResourceLoader_Locked(
    int64_t first_byte_position,
    int64_t last_byte_position) {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  reader_ = std::make_unique<MultiBufferReader>(
      url_data_->multibuffer(), first_byte_position, last_byte_position,
      is_client_audio_element_,
      base::BindRepeating(&MultiBufferDataSource::ProgressCallback, weak_ptr_),
      render_task_runner_);
  UpdateBufferSizes();
}

void MultiBufferDataSource::Initialize(InitializeCB init_cb) {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  DCHECK(init_cb);
  DCHECK(!reader_.get());

  init_cb_ = std::move(init_cb);

  CreateResourceLoader(0, kPositionNotSpecified);

  // We're not allowed to call Wait() if data is already available.
  if (reader_->Available()) {
    render_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MultiBufferDataSource::StartCallback, weak_ptr_));

    // When the entire file is already in the cache, we won't get any more
    // progress callbacks, which breaks some expectations. Post a task to
    // make sure that the client gets at least one call each for the progress
    // and loading callbacks.
    render_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MultiBufferDataSource::UpdateProgress,
                                  weak_factory_.GetWeakPtr()));
  } else {
    reader_->Wait(
        1, base::BindOnce(&MultiBufferDataSource::StartCallback, weak_ptr_));
  }
}

void MultiBufferDataSource::OnRedirected(
    const scoped_refptr<UrlData>& new_destination) {
  if (!new_destination || !url_data_) {
    // A failure occurred.
    failed_ = true;
    if (init_cb_) {
      render_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&MultiBufferDataSource::StartCallback, weak_ptr_));
    } else {
      base::AutoLock auto_lock(lock_);
      StopInternal_Locked();
    }
    StopLoader();
    return;
  }
  if (url_data_->url().DeprecatedGetOriginAsURL() !=
      new_destination->url().DeprecatedGetOriginAsURL()) {
    single_origin_ = false;
  }
  SetReader(nullptr);
  url_data_ = std::move(new_destination);

  url_data_->OnRedirect(
      base::BindOnce(&MultiBufferDataSource::OnRedirected, weak_ptr_));

  if (init_cb_) {
    CreateResourceLoader(0, kPositionNotSpecified);
    if (reader_->Available()) {
      render_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&MultiBufferDataSource::StartCallback, weak_ptr_));
    } else {
      reader_->Wait(
          1, base::BindOnce(&MultiBufferDataSource::StartCallback, weak_ptr_));
    }
  } else if (read_op_) {
    CreateResourceLoader(read_op_->position(), kPositionNotSpecified);
    if (reader_->Available()) {
      render_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&MultiBufferDataSource::ReadTask, weak_ptr_));
    } else {
      reader_->Wait(
          1, base::BindOnce(&MultiBufferDataSource::ReadTask, weak_ptr_));
    }
  }

  if (redirect_cb_)
    redirect_cb_.Run();
}

void MultiBufferDataSource::SetPreload(media::DataSource::Preload preload) {
  DVLOG(1) << __func__ << "(" << preload << ")";
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  preload_ = preload;
  UpdateBufferSizes();
}

bool MultiBufferDataSource::HasSingleOrigin() {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  // Before initialization completes there is no risk of leaking data. Callers
  // are required to order checks such that this isn't a race.
  return single_origin_;
}

bool MultiBufferDataSource::IsCorsCrossOrigin() const {
  return url_data_->is_cors_cross_origin();
}

void MultiBufferDataSource::OnRedirect(RedirectCB callback) {
  redirect_cb_ = std::move(callback);
}

bool MultiBufferDataSource::HasAccessControl() const {
  return url_data_->has_access_control();
}

bool MultiBufferDataSource::PassedTimingAllowOriginCheck() {
  return url_data_->passed_timing_allow_origin_check();
}

bool MultiBufferDataSource::WouldTaintOrigin() {
  // When the resource is redirected to another origin we think of it as
  // tainted. This is actually not specified, and is under discussion.
  // See https://github.com/whatwg/fetch/issues/737.
  if (!HasSingleOrigin() && cors_mode() == UrlData::CORS_UNSPECIFIED)
    return true;
  return IsCorsCrossOrigin();
}

UrlData::CorsMode MultiBufferDataSource::cors_mode() const {
  return url_data_->cors_mode();
}

void MultiBufferDataSource::OnMediaPlaybackRateChanged(double playback_rate) {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  if (playback_rate < 0 || playback_rate == playback_rate_)
    return;

  playback_rate_ = playback_rate;
  cancel_on_defer_ = false;
  UpdateBufferSizes();
}

void MultiBufferDataSource::OnMediaIsPlaying() {
  DCHECK(render_task_runner_->BelongsToCurrentThread());

  // Always clear this since it can be set by OnBufferingHaveEnough() calls at
  // any point in time.
  cancel_on_defer_ = false;

  if (media_has_played_)
    return;

  media_has_played_ = true;

  // Once we start playing, we need preloading.
  preload_ = AUTO;
  UpdateBufferSizes();
}

/////////////////////////////////////////////////////////////////////////////
// DataSource implementation.
void MultiBufferDataSource::Stop() {
  {
    base::AutoLock auto_lock(lock_);
    StopInternal_Locked();

    // Cleanup resources immediately if we're already on the right thread.
    if (render_task_runner_->BelongsToCurrentThread()) {
      reader_.reset();
      url_data_.reset();
      return;
    }
  }

  render_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MultiBufferDataSource::StopLoader,
                                weak_factory_.GetWeakPtr()));
}

void MultiBufferDataSource::Abort() {
  base::AutoLock auto_lock(lock_);
  DCHECK(!init_cb_);
  if (read_op_)
    ReadOperation::Run(std::move(read_op_), kAborted);

  // Abort does not call StopLoader() since it is typically called prior to a
  // seek or suspend. Let the loader logic make the decision about whether a new
  // loader is necessary upon the seek or resume.
}

void MultiBufferDataSource::SetBitrate(int bitrate) {
  render_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MultiBufferDataSource::SetBitrateTask,
                                weak_factory_.GetWeakPtr(), bitrate));
}

void MultiBufferDataSource::OnBufferingHaveEnough(bool always_cancel) {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  if (reader_ && (always_cancel || (preload_ == METADATA &&
                                    !media_has_played_ && !IsStreaming()))) {
    cancel_on_defer_ = true;
    if (!loading_) {
      base::AutoLock auto_lock(lock_);
      if (read_op_) {
        // We can't destroy the reader if a read operation is pending.
        // UpdateLoadingState_Locked will take care of it after the
        // operation is done.
        return;
      }
      // Already locked, no need to use SetReader().
      reader_.reset(nullptr);
    }
  }
}

int64_t MultiBufferDataSource::GetMemoryUsage() {
  // TODO(hubbe): Make more accurate when url_data_ is shared.
  return base::checked_cast<int64_t>(url_data_->CachedSize())
         << url_data_->multibuffer()->block_size_shift();
}

GURL MultiBufferDataSource::GetUrlAfterRedirects() const {
  return url_data_->url();
}

void MultiBufferDataSource::Read(int64_t position,
                                 int size,
                                 uint8_t* data,
                                 media::DataSource::ReadCB read_cb) {
  DVLOG(1) << "Read: " << position << " offset, " << size << " bytes";
  // Reading is not allowed until after initialization.
  DCHECK(!init_cb_);
  DCHECK(read_cb);

  {
    base::AutoLock auto_lock(lock_);
    DCHECK(!read_op_);

    if (stop_signal_received_) {
      std::move(read_cb).Run(kReadError);
      return;
    }

    // Optimization: Try reading from the cache here to get back to
    // muxing as soon as possible. This works because TryReadAt is
    // thread-safe.
    if (reader_) {
      int64_t bytes_read = reader_->TryReadAt(position, data, size);
      if (bytes_read > 0) {
        bytes_read_ += bytes_read;
        seek_positions_.push_back(position + bytes_read);
        if (seek_positions_.size() == 1) {
          render_task_runner_->PostDelayedTask(
              FROM_HERE,
              base::BindOnce(&MultiBufferDataSource::SeekTask,
                             weak_factory_.GetWeakPtr()),
              kSeekDelay);
        }

        std::move(read_cb).Run(static_cast<int>(bytes_read));
        return;
      }
    }
    read_op_ = std::make_unique<ReadOperation>(position, size, data,
                                               std::move(read_cb));
  }

  render_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(&MultiBufferDataSource::ReadTask,
                                               weak_factory_.GetWeakPtr()));
}

bool MultiBufferDataSource::GetSize(int64_t* size_out) {
  base::AutoLock auto_lock(lock_);
  if (total_bytes_ != kPositionNotSpecified) {
    *size_out = total_bytes_;
    return true;
  }
  *size_out = 0;
  return false;
}

bool MultiBufferDataSource::IsStreaming() {
  return streaming_;
}

/////////////////////////////////////////////////////////////////////////////
// This method is the place where actual read happens,
void MultiBufferDataSource::ReadTask() {
  DCHECK(render_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  if (stop_signal_received_ || !read_op_)
    return;
  DCHECK(read_op_->size());

  if (!reader_)
    CreateResourceLoader_Locked(read_op_->position(), kPositionNotSpecified);

  int64_t available = reader_->AvailableAt(read_op_->position());
  if (available < 0) {
    // A failure has occured.
    ReadOperation::Run(std::move(read_op_), kReadError);
    return;
  }
  if (available) {
    int64_t bytes_read = std::min<int64_t>(available, read_op_->size());
    bytes_read =
        reader_->TryReadAt(read_op_->position(), read_op_->data(), bytes_read);

    bytes_read_ += bytes_read;
    seek_positions_.push_back(read_op_->position() + bytes_read);

    if (bytes_read == 0 && total_bytes_ == kPositionNotSpecified) {
      // We've reached the end of the file and we didn't know the total size
      // before. Update the total size so Read()s past the end of the file will
      // fail like they would if we had known the file size at the beginning.
      total_bytes_ = read_op_->position() + bytes_read;
      if (total_bytes_ != kPositionNotSpecified)
        host_->SetTotalBytes(total_bytes_);
    }

    ReadOperation::Run(std::move(read_op_), static_cast<int>(bytes_read));

    SeekTask_Locked();
  } else {
    reader_->Seek(read_op_->position());
    reader_->Wait(1, base::BindOnce(&MultiBufferDataSource::ReadTask,
                                    weak_factory_.GetWeakPtr()));
    UpdateLoadingState_Locked(false);
  }
}

void MultiBufferDataSource::SeekTask() {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  SeekTask_Locked();
}

void MultiBufferDataSource::SeekTask_Locked() {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (stop_signal_received_)
    return;

  // A read operation is pending, which will call SeekTask_Locked when
  // it's done. We'll defer any seeking until the read op is done.
  if (read_op_)
    return;

  url_data_->AddBytesRead(bytes_read_);
  bytes_read_ = 0;

  if (reader_) {
    // If we're seeking to a new location, (not just slightly further
    // in the file) and we have more data buffered in that new location
    // than in our current location, then we don't actually seek anywhere.
    // Instead we keep preloading at the old location a while longer.

    int64_t pos = reader_->Tell();
    int64_t available = reader_->Available();

    // Iterate backwards, because if two positions have the same
    // amount of buffered data, we probably want to prefer the latest
    // one in the array.
    for (const auto& new_pos : base::Reversed(seek_positions_)) {
      int64_t available_at_new_pos = reader_->AvailableAt(new_pos);

      if (total_bytes_ != kPositionNotSpecified) {
        if (new_pos + available_at_new_pos >= total_bytes_) {
          // Buffer reaches end of file, no need to seek here.
          continue;
        }
      }
      if (available_at_new_pos < available) {
        pos = new_pos;
        available = available_at_new_pos;
      }
    }
    reader_->Seek(pos);
  }
  seek_positions_.clear();

  UpdateLoadingState_Locked(false);
}

void MultiBufferDataSource::StopInternal_Locked() {
  lock_.AssertAcquired();
  if (stop_signal_received_)
    return;

  stop_signal_received_ = true;

  // Initialize() isn't part of the DataSource interface so don't call it in
  // response to Stop().
  init_cb_.Reset();

  if (read_op_)
    ReadOperation::Run(std::move(read_op_), kReadError);
}

void MultiBufferDataSource::StopLoader() {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  SetReader(nullptr);
}

void MultiBufferDataSource::SetBitrateTask(int bitrate) {
  DCHECK(render_task_runner_->BelongsToCurrentThread());

  bitrate_ = bitrate;
  UpdateBufferSizes();
}

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader callback methods.
void MultiBufferDataSource::StartCallback() {
  DCHECK(render_task_runner_->BelongsToCurrentThread());

  // TODO(scherkus): we shouldn't have to lock to signal host(), see
  // http://crbug.com/113712 for details.
  base::AutoLock auto_lock(lock_);
  if (stop_signal_received_) {
    return;
  }

  if (!init_cb_) {
    // Can't call SetReader(nullptr) since we are holding the lock.
    reader_.reset(nullptr);
    return;
  }

  // All responses must be successful. Resources that are assumed to be fully
  // buffered must have a known content length.
  bool success =
      reader_ && reader_->Available() > 0 && url_data_ &&
      (!AssumeFullyBuffered() || url_data_->length() != kPositionNotSpecified);

  if (success) {
    total_bytes_ = url_data_->length();
    streaming_ =
        !AssumeFullyBuffered() && (total_bytes_ == kPositionNotSpecified ||
                                   !url_data_->range_supported());

    media_log_->SetProperty<media::MediaLogProperty::kTotalBytes>(total_bytes_);
    media_log_->SetProperty<media::MediaLogProperty::kIsStreaming>(streaming_);
  } else {
    // Can't call SetReader(nullptr) since we are holding the lock.
    reader_.reset(nullptr);
  }

  if (success) {
    if (total_bytes_ != kPositionNotSpecified) {
      host_->SetTotalBytes(total_bytes_);
      if (AssumeFullyBuffered())
        host_->AddBufferedByteRange(0, total_bytes_);
    }

    // Progress callback might be called after the start callback,
    // make sure that we update single_origin_ now.
    media_log_->SetProperty<media::MediaLogProperty::kIsSingleOrigin>(
        single_origin_);
    media_log_->SetProperty<media::MediaLogProperty::kIsRangeHeaderSupported>(
        url_data_->range_supported());
  }

  render_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(init_cb_), success));

  UpdateBufferSizes();

  // Even if data is cached, say that we're loading at this point for
  // compatibility.
  UpdateLoadingState_Locked(true);
}

void MultiBufferDataSource::ProgressCallback(int64_t begin, int64_t end) {
  DVLOG(1) << __func__ << "(" << begin << ", " << end << ")";
  DCHECK(render_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  if (stop_signal_received_)
    return;

  if (AssumeFullyBuffered())
    return;

  if (end > begin)
    host_->AddBufferedByteRange(begin, end);

  if (buffer_size_update_counter_ > 0)
    buffer_size_update_counter_--;
  else
    UpdateBufferSizes();

  UpdateLoadingState_Locked(false);
}

void MultiBufferDataSource::UpdateLoadingState_Locked(bool force_loading) {
  DVLOG(1) << __func__;
  lock_.AssertAcquired();
  if (AssumeFullyBuffered())
    return;
  // Update loading state.
  bool is_loading = !!reader_ && reader_->IsLoading();
  if (force_loading || is_loading != loading_) {
    bool loading = is_loading || force_loading;

    if (!loading && cancel_on_defer_) {
      if (read_op_) {
        // We can't destroy the reader if a read operation is pending.
        // UpdateLoadingState_Locked will be called again when the read
        // operation is done.
        return;
      }
      // Already locked, no need to use SetReader().
      reader_.reset(nullptr);
    }

    loading_ = loading;
    downloading_cb_.Run(loading_);
  }
}

void MultiBufferDataSource::UpdateProgress() {
  DCHECK(render_task_runner_->BelongsToCurrentThread());
  if (reader_) {
    uint64_t available = reader_->Available();
    uint64_t pos = reader_->Tell();
    ProgressCallback(pos, pos + available);
  }
}

void MultiBufferDataSource::UpdateBufferSizes() {
  DVLOG(1) << __func__;
  if (!reader_)
    return;

  buffer_size_update_counter_ = kUpdateBufferSizeFrequency;

  // Use a default bit rate if unknown and clamp to prevent overflow.
  int64_t bitrate = std::clamp<int64_t>(bitrate_, 0, kMaxBitrate);
  if (bitrate == 0)
    bitrate = kDefaultBitrate;

  // Only scale the buffer window for playback rates greater than 1.0 in
  // magnitude and clamp to prevent overflow.
  double playback_rate = playback_rate_;

  playback_rate = std::max(playback_rate, 1.0);
  playback_rate = std::min(playback_rate, kMaxPlaybackRate);

  int64_t bytes_per_second = (bitrate / 8.0) * playback_rate;

  // Preload 10 seconds of data, clamped to some min/max value.
  int64_t preload = std::clamp(preload_seconds_.value() * bytes_per_second,
                               kMinBufferPreload, kMaxBufferPreload);

  // Increase buffering slowly at a rate of 10% of data downloaded so
  // far, maxing out at the preload size.
  int64_t extra_buffer = std::min(
      preload, url_data_->BytesReadFromCache() * kSlowPreloadPercentage / 100);

  // Add extra buffer to preload.
  preload += extra_buffer;

  // We preload this much, then we stop unil we read |preload| before resuming.
  int64_t preload_high = preload + kPreloadHighExtra;

  // We pin a few seconds of data behind the current reading position.
  int64_t pin_backward =
      std::clamp(keep_after_playback_seconds_.value() * bytes_per_second,
                 kMinBufferPreload, kMaxBufferPreload);

  // We always pin at least kDefaultPinSize ahead of the read position.
  // Normally, the extra space between preload_high and kDefaultPinSize will
  // not actually have any data in it, but if it does, we don't want to throw it
  // away right before we need it.
  int64_t pin_forward = std::max(preload_high, kDefaultPinSize);

  // Note that the buffer size is advisory as only non-pinned data is allowed
  // to be thrown away. Most of the time we pin a region that is larger than
  // |buffer_size|, which only makes sense because most of the time, some of
  // the data in pinned region is not present in the cache.
  int64_t buffer_size = std::min(
      (preload_seconds_.value() + keep_after_playback_seconds_.value()) *
              bytes_per_second +
          extra_buffer * 3,
      preload_high + pin_backward + extra_buffer);

  if (url_data_->FullyCached() ||
      (url_data_->length() != kPositionNotSpecified &&
       url_data_->length() < kDefaultPinSize)) {
    // We just make pin_forwards/backwards big enough to encompass the
    // whole file regardless of where we are, with some extra margins.
    pin_forward = std::max(pin_forward, url_data_->length() * 2);
    pin_backward = std::max(pin_backward, url_data_->length() * 2);
    buffer_size = url_data_->length();
  }

  reader_->SetMaxBuffer(buffer_size);
  reader_->SetPinRange(pin_backward, pin_forward);

  if (preload_ == METADATA) {
    preload_high >>= kMetadataShift;
    preload >>= kMetadataShift;
  }
  reader_->SetPreload(preload_high, preload);
}

}  // namespace blink
