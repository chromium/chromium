// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/audio/input_sync_writer.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/audio_glitch_info.h"
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
    bool key_pressed,
    base::TimeTicks capture_time,
    const media::AudioGlitchInfo& glitch_info,
    std::unique_ptr<media::AudioBus> audio_bus)
    : volume_(volume),
      key_pressed_(key_pressed),
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
    base::MappedReadOnlyRegion shared_memory,
    std::unique_ptr<base::CancelableSyncSocket> socket,
    uint32_t shared_memory_segment_count,
    const media::AudioParameters& params,
    std::unique_ptr<InputGlitchCounter> glitch_counter)
    : log_callback_(std::move(log_callback)),
      socket_(std::move(socket)),
      shared_memory_region_(std::move(shared_memory.region)),
      shared_memory_mapping_(std::move(shared_memory.mapping)),
      shared_memory_segment_size_([&]() {
        CHECK(shared_memory_segment_count > 0);
        return shared_memory_mapping_.size() / shared_memory_segment_count;
      }()),
      creation_time_(base::TimeTicks::Now()),
      audio_bus_memory_size_(media::AudioBus::CalculateMemorySize(params)),
      glitch_counter_(std::move(glitch_counter)),
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
  DVLOG(1) << "shared memory size: " << shared_memory_mapping_.size();
  DVLOG(1) << "shared memory segment count: " << shared_memory_segment_count;
  DVLOG(1) << "audio bus memory size: " << audio_bus_memory_size_;
  DCHECK(glitch_counter_);

  audio_buses_.resize(shared_memory_segment_count);

  // Create vector of audio buses by wrapping existing blocks of memory.
  uint8_t* ptr = static_cast<uint8_t*>(shared_memory_mapping_.memory());
  CHECK(ptr);
  for (auto& bus : audio_buses_) {
    CHECK_EQ(0U, reinterpret_cast<uintptr_t>(ptr) &
                     (media::AudioBus::kChannelAlignment - 1));
    media::AudioInputBuffer* buffer =
        reinterpret_cast<media::AudioInputBuffer*>(ptr);
    bus = media::AudioBus::WrapMemory(params, buffer->audio);
    ptr += shared_memory_segment_size_;
  }
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

  // Make sure we can share the memory read-only with the client.
  auto shared_memory = base::ReadOnlySharedMemoryRegion::Create(
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

base::ReadOnlySharedMemoryRegion InputSyncWriter::TakeSharedMemoryRegion() {
  DCHECK(shared_memory_region_.IsValid());
  return std::move(shared_memory_region_);
}

void InputSyncWriter::Write(const media::AudioBus* data,
                            double volume,
                            bool key_pressed,
                            base::TimeTicks capture_time,
                            const media::AudioGlitchInfo& glitch_info) {
  TRACE_EVENT("audio", "InputSyncWriter::Write", "capture_time (ms)",
              (capture_time - base::TimeTicks()).InMillisecondsF(),
              "capture_delay (ms)",
              (base::TimeTicks::Now() - capture_time).InMillisecondsF());
  glitch_info.MaybeAddTraceEvent();

  CheckTimeSinceLastWrite();

  pending_glitch_info_ += glitch_info;

  // Check that the renderer side has read data so that we don't overwrite data
  // that hasn't been read yet. The renderer side sends a signal over the socket
  // each time it has read data. Here, we read those verifications before
  // writing. We verify that each buffer index is in sequence.
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
          *data_it->audio_bus_, data_it->volume_, data_it->key_pressed_,
          data_it->capture_time_, data_it->glitch_info_);
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
      static const char* message = "AISW: Fifo emptied.";
      log_callback_.Run(message);
    }
  }

  // Write the current data to the shared memory if there is room, otherwise
  // put it in the fifo.
  if (number_of_filled_segments_ < audio_buses_.size()) {
    DCHECK(overflow_data_.empty());
    const bool successful_write = WriteDataToCurrentSegment(
        *data, volume, key_pressed, capture_time, pending_glitch_info_);
    glitch_counter_->ReportDroppedData(!successful_write);
    if (successful_write) {
      pending_glitch_info_ = {};
    } else {
      pending_glitch_info_ += dropped_buffer_glitch_;
    }
  } else {
    if (PushDataToFifo(*data, volume, key_pressed, capture_time,
                       pending_glitch_info_)) {
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
  std::ostringstream oss;
  if (last_write_time_.is_null()) {
    // This is the first time Write is called.
    base::TimeDelta interval = new_write_time - creation_time_;
    oss << "AISW::Write: audio input data received for the first time: delay "
           "= "
        << interval.InMilliseconds() << "ms";
  } else {
    base::TimeDelta interval = new_write_time - last_write_time_;
    if (interval > kLogDelayThreadhold) {
      oss << "AISW::Write: audio input data delay unexpectedly long: delay = "
          << interval.InMilliseconds() << "ms";
    }
  }
  const std::string log_message = oss.str();
  if (!log_message.empty()) {
    log_callback_.Run(log_message);
  }

  last_write_time_ = new_write_time;
#endif
}

bool InputSyncWriter::PushDataToFifo(
    const media::AudioBus& data,
    double volume,
    bool key_pressed,
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
      static const char* error_message = "AISW: No room in fifo.";
      LOG(WARNING) << error_message;
      log_callback_.Run(error_message);
      if (fifo_full_count_ == 50) {
        static const char* cap_error_message =
            "AISW: Log cap reached, suppressing further fifo overflow logs.";
        LOG(WARNING) << cap_error_message;
        log_callback_.Run(error_message);
      }
    }
    ++fifo_full_count_;
    return false;
  }

  if (overflow_data_.empty()) {
    static const char* message = "AISW: Starting to use fifo.";
    log_callback_.Run(message);
  }

  // Push data to fifo.
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(data.channels(), data.frames());
  data.CopyTo(audio_bus.get());
  overflow_data_.emplace_back(volume, key_pressed, capture_time, glitch_info,
                              std::move(audio_bus));
  DCHECK_LE(overflow_data_.size(), static_cast<size_t>(kMaxOverflowBusesSize));
  return true;
}

bool InputSyncWriter::WriteDataToCurrentSegment(
    const media::AudioBus& data,
    double volume,
    bool key_pressed,
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
  buffer->params.key_pressed = key_pressed;
  buffer->params.capture_time_us =
      (capture_time - base::TimeTicks()).InMicroseconds();
  buffer->params.id = next_buffer_id_;
  buffer->params.glitch_duration_us = glitch_info.duration.InMicroseconds();
  buffer->params.glitch_count = glitch_info.count;

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
      static const char* error_message = "AISW: No room in socket buffer.";
      PLOG(WARNING) << error_message;
      log_callback_.Run(error_message);
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
    uint32_t segment_id) const {
  uint8_t* ptr = static_cast<uint8_t*>(shared_memory_mapping_.memory());
  CHECK_LT(segment_id, audio_buses_.size());
  ptr += segment_id * shared_memory_segment_size_;
  return reinterpret_cast<media::AudioInputBuffer*>(ptr);
}

}  // namespace audio
