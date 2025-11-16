// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/input_sync_writer.h"

#include <algorithm>
#include <cinttypes>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/media_switches.h"
#include "services/audio/input_glitch_counter.h"

namespace audio {

namespace {

// Used to log if any audio glitches have been detected during an audio session.
// Elements in this enum should not be added, deleted or rearranged.
enum class AudioGlitchResult {
  kNoGlitches = 0,
  kGlitches = 1,
  kMaxValue = kGlitches
};

}  // namespace

InputSyncWriter::OverflowData::OverflowData(
    double volume,
    base::TimeTicks capture_time,
    const media::AudioGlitchInfo& glitch_info,
    std::unique_ptr<media::AudioBus> audio_bus)
    : volume_(volume),
      capture_time_(capture_time),
      glitch_info_(glitch_info),
      audio_bus_(std::move(audio_bus)) {}
InputSyncWriter::OverflowData::~OverflowData() {}
InputSyncWriter::OverflowData::OverflowData(InputSyncWriter::OverflowData&&) =
    default;
InputSyncWriter::OverflowData& InputSyncWriter::OverflowData::operator=(
    InputSyncWriter::OverflowData&& other) = default;

InputSyncWriter::InputSyncWriter(
    base::RepeatingCallback<void(const std::string&)> log_callback,
    base::UnsafeSharedMemoryRegion shared_memory,
    std::unique_ptr<base::CancelableSyncSocket> socket,
    uint32_t shared_memory_segment_count,
    const media::AudioParameters& params,
    std::unique_ptr<InputGlitchCounter> glitch_counter)
    : log_callback_(std::move(log_callback)),
      socket_(std::move(socket)),
      shared_memory_region_(std::move(shared_memory)),
      shared_memory_mapping_(shared_memory_region_.Map()),
      shared_memory_segment_size_([&]() {
        CHECK(shared_memory_segment_count > 0);
        return shared_memory_mapping_.size() / shared_memory_segment_count;
      }()),
      creation_time_(base::TimeTicks::Now()),
      audio_bus_memory_size_(base::checked_cast<uint32_t>(
          media::AudioBus::CalculateMemorySize(params))),
      glitch_counter_(std::move(glitch_counter)),
      confirm_reads_via_shmem_(
          base::FeatureList::IsEnabled(media::kAudioInputConfirmReadsViaShmem)),
      dropped_buffer_glitch_{.duration = params.GetBufferDuration(),
                             .count = 1} {
  // We use CHECKs since this class is used for IPC.
  DCHECK(log_callback_);
  CHECK(socket_);
  CHECK(shared_memory_region_.IsValid());
  CHECK(shared_memory_mapping_.IsValid());
  CHECK_EQ(shared_memory_segment_size_ * shared_memory_segment_count,
           shared_memory_mapping_.size());
  CHECK_EQ(shared_memory_segment_size_,
           audio_bus_memory_size_ + sizeof(media::AudioInputBufferParameters));
  SendLogMessage("%s({shared_memory_segment_count=%u}, {params=%s})", __func__,
                 shared_memory_segment_count,
                 params.AsHumanReadableString().c_str());
  SendLogMessage(
      "%s => (shared_memory_segment_size=[%u], audio_bus_memory_size=[%u])",
      __func__, shared_memory_segment_size_, audio_bus_memory_size_);
  DCHECK(glitch_counter_);

  audio_buses_.resize(shared_memory_segment_count);

  // Create vector of audio buses by wrapping existing blocks of memory.
  base::span<uint8_t> data = shared_memory_mapping_.GetMemoryAsSpan<uint8_t>();
  CHECK(!data.empty());
  auto reader = base::SpanReader<uint8_t>(data);

  for (auto& bus : audio_buses_) {
    auto input_buffer = *reader.Read(shared_memory_segment_size_);
    auto audio_data =
        input_buffer.subspan<sizeof(media::AudioInputBufferParameters)>();
    CHECK_EQ(audio_data.size(), audio_bus_memory_size_);
    CHECK(
        base::IsAligned(audio_data.data(), media::AudioBus::kChannelAlignment));
    bus = media::AudioBus::WrapMemory(params, audio_data);
  }

  CHECK(reader.remaining_span().empty());
}

InputSyncWriter::~InputSyncWriter() = default;

// static
std::unique_ptr<InputSyncWriter> InputSyncWriter::Create(
    base::RepeatingCallback<void(const std::string&)> log_callback,
    uint32_t shared_memory_segment_count,
    const media::AudioParameters& params,
    base::CancelableSyncSocket* foreign_socket) {
  // Having no shared memory doesn't make sense, so fail creation in that case.
  if (shared_memory_segment_count == 0)
    return nullptr;

  base::CheckedNumeric<uint32_t> requested_memory_size =
      ComputeAudioInputBufferSizeChecked(params, shared_memory_segment_count);

  if (!requested_memory_size.IsValid())
    return nullptr;

  auto shared_memory = base::UnsafeSharedMemoryRegion::Create(
      requested_memory_size.ValueOrDie());
  if (!shared_memory.IsValid())
    return nullptr;

  auto socket = std::make_unique<base::CancelableSyncSocket>();
  if (!base::CancelableSyncSocket::CreatePair(socket.get(), foreign_socket)) {
    return nullptr;
  }

  auto glitch_counter = std::make_unique<InputGlitchCounter>(log_callback);

  return std::make_unique<InputSyncWriter>(
      std::move(log_callback), std::move(shared_memory), std::move(socket),
      shared_memory_segment_count, params, std::move(glitch_counter));
}

base::UnsafeSharedMemoryRegion InputSyncWriter::TakeSharedMemoryRegion() {
  DCHECK(shared_memory_region_.IsValid());
  return std::move(shared_memory_region_);
}

void InputSyncWriter::Write(const media::AudioBus* data,
                            double volume,
                            base::TimeTicks capture_time,
                            const media::AudioGlitchInfo& glitch_info) {
  TRACE_EVENT("audio", "InputSyncWriter::Write", "capture_time (ms)",
              (capture_time - base::TimeTicks()).InMillisecondsF(),
              "capture_delay (ms)",
              (base::TimeTicks::Now() - capture_time).InMillisecondsF());
  glitch_info.MaybeAddTraceEvent();

  CheckTimeSinceLastWrite();

  pending_glitch_info_ += glitch_info;

  ReceiveReadConfirmationsFromConsumer();

  const size_t segment_count = audio_buses_.size();
  // If the shared memory is full, then we consider the deadline to be missed.
  glitch_counter_->ReportMissedReadDeadline(number_of_filled_segments_ ==
                                            segment_count);

  // If there is data in the fifo, write as much of it to shared memory as
  // possible.
  if (!overflow_data_.empty()) {
    auto data_it = overflow_data_.begin();

    while (data_it != overflow_data_.end() &&
           number_of_filled_segments_ < segment_count) {
      // Write parameters to shared memory, and report whether it was dropped.
      const bool successful_write = WriteDataToCurrentSegment(
          *data_it->audio_bus_, data_it->volume_, data_it->capture_time_,
          data_it->glitch_info_);
      glitch_counter_->ReportDroppedData(!successful_write);
      if (!successful_write) {
        // The glitch info was not written successfully, we need to keep it to
        // write it in the future.
        pending_glitch_info_ += data_it->glitch_info_;
        pending_glitch_info_ += dropped_buffer_glitch_;
      }
      ++data_it;
    }

    // Erase all copied data from fifo.
    overflow_data_.erase(overflow_data_.begin(), data_it);

    if (overflow_data_.empty()) {
      SendLogMessage("%s => (FIFO emptied)", __func__);
    }
  }

  // Write the current data to the shared memory if there is room, otherwise
  // put it in the fifo.
  if (number_of_filled_segments_ < audio_buses_.size()) {
    DCHECK(overflow_data_.empty());
    const bool successful_write = WriteDataToCurrentSegment(
        *data, volume, capture_time, pending_glitch_info_);
    glitch_counter_->ReportDroppedData(!successful_write);
    if (successful_write) {
      pending_glitch_info_ = {};
    } else {
      pending_glitch_info_ += dropped_buffer_glitch_;
    }
  } else {
    if (PushDataToFifo(*data, volume, capture_time, pending_glitch_info_)) {
      pending_glitch_info_ = {};
    } else {
      glitch_counter_->ReportDroppedData(true);
      pending_glitch_info_ += dropped_buffer_glitch_;
    }
  }
}

void InputSyncWriter::Close() {
  socket_->Close();
}

void InputSyncWriter::CheckTimeSinceLastWrite() {
#if !BUILDFLAG(IS_ANDROID)
  static const base::TimeDelta kLogDelayThreadhold = base::Milliseconds(500);

  base::TimeTicks new_write_time = base::TimeTicks::Now();
  if (last_write_time_.is_null()) {
    // This is the first time Write is called.
    base::TimeDelta interval = new_write_time - creation_time_;
    SendLogMessage(
        "%s => (audio input data received for the first time: delay=%" PRId64
        " ms)",
        __func__, interval.InMilliseconds());
  } else {
    base::TimeDelta interval = new_write_time - last_write_time_;
    if (interval > kLogDelayThreadhold) {
      SendLogMessage(
          "%s => (WARNING: audio input data delay unexpectedly long: "
          "delay=%" PRId64 " ms)",
          __func__, interval.InMilliseconds());
    }
  }

  last_write_time_ = new_write_time;
#endif
}

void InputSyncWriter::ReceiveReadConfirmationsFromConsumer() {
  // This function confirms how much data the consumer has read, in order to
  // update how much available space we have in shared memory. It does either by
  // reading confirmations in shared memory or by reading confirmations from the
  // socket, depending on the value of `confirm_reads_via_shmem_`.

  if (confirm_reads_via_shmem_) {
    // Experimental read confirmation mechanism.
    // When the InputSyncWriter has written an audio buffer to a segment in
    // shared memory, it sets an atomic flag `has_unread_data` in that segment
    // to 1. When the consumer side has read the data, it resets
    // `has_unread_data` back to 0 as a read confirmation.

    // We loop forward until we meet the first segment with unread audio, or
    // until we know that the consumer side has read all segments that we have
    // written to.
    while (next_read_buffer_index_ < next_buffer_id_) {
      // The next buffer we expect to read a confirmation from.
      media::AudioInputBuffer* buffer =
          GetSharedInputBuffer(next_read_buffer_index_ % audio_buses_.size());
      std::atomic_ref<uint32_t> has_unread_data(buffer->params.has_unread_data);
      // If this buffer has been read by the consumer side, it will have set the
      // `has_unread_data` flag to 0.
      if (has_unread_data.load(std::memory_order_relaxed)) {
        break;
      }
      ++next_read_buffer_index_;
      CHECK_GT(number_of_filled_segments_, 0u);
      --number_of_filled_segments_;
    }
    return;
  }
  // Old read confirmation mechanism.
  // When the InputSyncWriter has written an audio buffer to a segment in
  // shared memory, it sends the index of that audio buffer over the socket.
  // When the consumer side has read the data, it sends the index of the next
  // buffer it wants to write back over the socket as a read confirmation.

  // Read as many confirmations from the socket as are available, assert that
  // they are in order, and update the number of filled segments.
  size_t number_of_indices_available = socket_->Peek() / sizeof(uint32_t);
  if (number_of_indices_available > 0) {
    auto indices =
        base::HeapArray<uint32_t>::WithSize(number_of_indices_available);
    size_t bytes_received =
        socket_->Receive(base::as_writable_bytes(indices.as_span()));
    CHECK_EQ(number_of_indices_available * sizeof(indices[0]), bytes_received);
    for (size_t i = 0; i < number_of_indices_available; ++i) {
      ++next_read_buffer_index_;
      CHECK_EQ(indices[i], next_read_buffer_index_);
      CHECK_GT(number_of_filled_segments_, 0u);
      --number_of_filled_segments_;
    }
  }
}

bool InputSyncWriter::PushDataToFifo(
    const media::AudioBus& data,
    double volume,
    base::TimeTicks capture_time,
    const media::AudioGlitchInfo& glitch_info) {
  TRACE_EVENT("audio", "InputSyncWriter::PushDataToFifo", "capture time (ms)",
              (capture_time - base::TimeTicks()).InMillisecondsF(),
              "capture_delay (ms)",
              (base::TimeTicks::Now() - capture_time).InMillisecondsF(),
              "fifo delay (ms)",
              (number_of_filled_segments_ + overflow_data_.size()) *
                  dropped_buffer_glitch_.duration);
  if (overflow_data_.size() == kMaxOverflowBusesSize) {
    TRACE_EVENT_INSTANT0(
        "audio", "InputSyncWriter::PushDataToFifo - overflow - dropped data",
        TRACE_EVENT_SCOPE_THREAD);
    if (fifo_full_count_ <= 50 && fifo_full_count_ % 10 == 0) {
      SendLogMessage("%s => (WARNING: no room in FIFO)", __func__);
      if (fifo_full_count_ == 50) {
        SendLogMessage(
            "%s => (WARNING: log cap reached, suppressing further FIFO "
            "overflow logs)",
            __func__);
      }
    }
    ++fifo_full_count_;
    return false;
  }

  if (overflow_data_.empty()) {
    SendLogMessage("%s => (starting to use the FIFO)", __func__);
  }

  // Push data to fifo.
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(data.channels(), data.frames());
  data.CopyTo(audio_bus.get());
  overflow_data_.emplace_back(volume, capture_time, glitch_info,
                              std::move(audio_bus));
  DCHECK_LE(overflow_data_.size(), static_cast<size_t>(kMaxOverflowBusesSize));
  return true;
}

bool InputSyncWriter::WriteDataToCurrentSegment(
    const media::AudioBus& data,
    double volume,
    base::TimeTicks capture_time,
    const media::AudioGlitchInfo& glitch_info) {
  CHECK(number_of_filled_segments_ < audio_buses_.size());

  TRACE_EVENT("audio", "WriteDataToCurrentSegment", "glitches",
              glitch_info.count, "glitch_duration (ms)",
              glitch_info.duration.InMillisecondsF(), "capture_time (ms)",
              (capture_time - base::TimeTicks()).InMillisecondsF(),
              "capture_delay (ms)",
              (base::TimeTicks::Now() - capture_time).InMillisecondsF(),
              "fifo delay (ms)",
              number_of_filled_segments_ * dropped_buffer_glitch_.duration);
  media::AudioInputBuffer* buffer = GetSharedInputBuffer(current_segment_id_);
  buffer->params.volume = volume;
  buffer->params.size = audio_bus_memory_size_;
  buffer->params.capture_time_us =
      (capture_time - base::TimeTicks()).InMicroseconds();
  buffer->params.id = next_buffer_id_;
  buffer->params.glitch_duration_us = glitch_info.duration.InMicroseconds();
  buffer->params.glitch_count = glitch_info.count;

  if (confirm_reads_via_shmem_) {
    // Part of the experimental synchronization mechanism. We will not write
    // more data to this buffer until the consumer side has set this flag back
    // to 0.
    std::atomic_ref<uint32_t> has_unread_data(buffer->params.has_unread_data);
    has_unread_data.store(1, std::memory_order_relaxed);
  }

  // Copy data into shared memory using pre-allocated audio buses.
  data.CopyTo(audio_buses_[current_segment_id_].get());

  return SignalDataWrittenAndUpdateCounters();
}

bool InputSyncWriter::SignalDataWrittenAndUpdateCounters() {
  if (socket_->Send(base::byte_span_from_ref(current_segment_id_)) !=
      sizeof(current_segment_id_)) {
    // Ensure we don't log consecutive errors as this can lead to a large
    // amount of logs.
    if (!had_socket_error_) {
      had_socket_error_ = true;
      SendLogMessage("%s => (WARNING: no room in socket buffer, dropped data)",
                     __func__);
      TRACE_EVENT_INSTANT0(
          "audio", "InputSyncWriter: No room in socket buffer - dropped data",
          TRACE_EVENT_SCOPE_THREAD);
    }
    return false;
  }
  had_socket_error_ = false;

  if (++current_segment_id_ >= audio_buses_.size())
    current_segment_id_ = 0;
  ++number_of_filled_segments_;
  CHECK_LE(number_of_filled_segments_, audio_buses_.size());
  ++next_buffer_id_;

  return true;
}

media::AudioInputBuffer* InputSyncWriter::GetSharedInputBuffer(
    uint32_t segment_id) {
  uint8_t* ptr = static_cast<uint8_t*>(shared_memory_mapping_.memory());
  CHECK_LT(segment_id, audio_buses_.size());
  UNSAFE_TODO(ptr += segment_id * shared_memory_segment_size_);
  return reinterpret_cast<media::AudioInputBuffer*>(ptr);
}

void InputSyncWriter::SendLogMessage(const char* format, ...) {
  if (log_callback_.is_null()) {
    return;
  }
  va_list args;
  va_start(args, format);
  log_callback_.Run(
      base::StrCat({"AISW::", base::StringPrintV(format, args),
                    base::StringPrintf(" [this=0x%" PRIXPTR "]",
                                       reinterpret_cast<uintptr_t>(this))}));
  va_end(args);
}

}  // namespace audio
