// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/input_sync_writer.h"

#include <algorithm>
#include <utility>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

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
    std::unique_ptr<media::AudioBus> audio_bus)
    : volume_(volume),
      key_pressed_(key_pressed),
      capture_time_(capture_time),
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
    const media::AudioParameters& params)
    : log_callback_(std::move(log_callback)),
      socket_(std::move(socket)),
      shared_memory_region_(std::move(shared_memory.region)),
      shared_memory_mapping_(std::move(shared_memory.mapping)),
      shared_memory_segment_size_(
          (CHECK(shared_memory_segment_count > 0),
           shared_memory_mapping_.size() / shared_memory_segment_count)),
      creation_time_(base::TimeTicks::Now()),
      audio_bus_memory_size_(media::AudioBus::CalculateMemorySize(params)) {
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

InputSyncWriter::~InputSyncWriter() {
  // We log the following:
  // - Percentage of data written to fifo (and not to shared memory).
  // - Percentage of data dropped (fifo reached max size or socket buffer full).
  // - Glitch yes/no (at least 1 drop).
  //
  // Subtract 'trailing' counts that will happen if the renderer process was
  // killed or e.g. the page refreshed while the input device was open etc.
  // This trims off the end of both the error and write counts so that we
  // preserve the proportion of counts before the teardown period. We pick
  // the largest trailing count as the time we consider that the trailing errors
  // begun, and subract that from the total write count.
  DCHECK_LE(trailing_write_to_fifo_count_, write_to_fifo_count_);
  DCHECK_LE(trailing_write_to_fifo_count_, write_count_);
  DCHECK_LE(trailing_write_error_count_, write_error_count_);
  DCHECK_LE(trailing_write_error_count_, write_count_);

  write_to_fifo_count_ -= trailing_write_to_fifo_count_;
  write_error_count_ -= trailing_write_error_count_;
  write_count_ -=
      std::max(trailing_write_to_fifo_count_, trailing_write_error_count_);

  if (write_count_ == 0)
    return;

  base::UmaHistogramPercentage("Media.AudioCapturerMissedReadDeadline",
                               100.0 * write_to_fifo_count_ / write_count_);

  base::UmaHistogramPercentage("Media.AudioCapturerDroppedData",
                               100.0 * write_error_count_ / write_count_);

  base::UmaHistogramEnumeration("Media.AudioCapturerAudioGlitches",
                                write_error_count_ == 0
                                    ? AudioGlitchResult::kNoGlitches
                                    : AudioGlitchResult::kGlitches);

  const int kPermilleScaling = 1000;
  // 10%: if we have more that 10% of callbacks having issues, the details are
  // not very interesting any more, so we just log all those cases together to
  // have a better resolution for lower values.
  const int kHistogramRange = kPermilleScaling / 10;

  // 30 s for 10 ms buffers.
  const int kShortStreamMaxCallbackCount = 3000;
  const std::string suffix =
      write_count_ < kShortStreamMaxCallbackCount ? "Short" : "Long";

  int missed_deadline =
      std::ceil(kPermilleScaling * static_cast<double>(write_to_fifo_count_) /
                write_count_);

  base::UmaHistogramCustomCounts(
      "Media.AudioCapturerMissedReadDeadline2." + suffix,
      std::min(missed_deadline, kHistogramRange), 0, kHistogramRange + 1, 100);

  int dropped_data =
      std::ceil(kPermilleScaling * static_cast<double>(write_error_count_) /
                write_count_);

  base::UmaHistogramCustomCounts("Media.AudioCapturerDroppedData2." + suffix,
                                 std::min(dropped_data, kHistogramRange), 0,
                                 kHistogramRange + 1, 100);

  std::string log_string = base::StringPrintf(
      "AISW: number of detected audio glitches: %" PRIuS " out of %" PRIuS,
      write_error_count_, write_count_);
  log_callback_.Run(log_string);
}

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

  return std::make_unique<InputSyncWriter>(
      std::move(log_callback), std::move(shared_memory), std::move(socket),
      shared_memory_segment_count, params);
}

base::ReadOnlySharedMemoryRegion InputSyncWriter::TakeSharedMemoryRegion() {
  DCHECK(shared_memory_region_.IsValid());
  return std::move(shared_memory_region_);
}

void InputSyncWriter::Write(const media::AudioBus* data,
                            double volume,
                            bool key_pressed,
                            base::TimeTicks capture_time) {
  TRACE_EVENT1("audio", "InputSyncWriter::Write", "capture time (ms)",
               (capture_time - base::TimeTicks()).InMillisecondsF());
  ++write_count_;
  CheckTimeSinceLastWrite();

  // Check that the renderer side has read data so that we don't overwrite data
  // that hasn't been read yet. The renderer side sends a signal over the socket
  // each time it has read data. Here, we read those verifications before
  // writing. We verify that each buffer index is in sequence.
  size_t number_of_indices_available = socket_->Peek() / sizeof(uint32_t);
  if (number_of_indices_available > 0) {
    auto indices = std::make_unique<uint32_t[]>(number_of_indices_available);
    size_t bytes_received = socket_->Receive(
        &indices[0], number_of_indices_available * sizeof(indices[0]));
    CHECK_EQ(number_of_indices_available * sizeof(indices[0]), bytes_received);
    for (size_t i = 0; i < number_of_indices_available; ++i) {
      ++next_read_buffer_index_;
      CHECK_EQ(indices[i], next_read_buffer_index_);
      CHECK_GT(number_of_filled_segments_, 0u);
      --number_of_filled_segments_;
    }
  }

  bool write_error = !WriteDataFromFifoToSharedMemory();

  // Write the current data to the shared memory if there is room, otherwise
  // put it in the fifo.
  if (number_of_filled_segments_ < audio_buses_.size()) {
    WriteParametersToCurrentSegment(volume, key_pressed, capture_time);

    // Copy data into shared memory using pre-allocated audio buses.
    data->CopyTo(audio_buses_[current_segment_id_].get());

    if (!SignalDataWrittenAndUpdateCounters())
      write_error = true;

    trailing_write_to_fifo_count_ = 0;
  } else {
    if (!PushDataToFifo(data, volume, key_pressed, capture_time))
      write_error = true;

    ++write_to_fifo_count_;
    ++trailing_write_to_fifo_count_;
  }

  // Increase write error counts if error, or reset the trailing error counter
  // if all write operations went well (no data dropped).
  if (write_error) {
    ++write_error_count_;
    ++trailing_write_error_count_;
    TRACE_EVENT_INSTANT0("audio", "InputSyncWriter write error",
                         TRACE_EVENT_SCOPE_THREAD);
  } else {
    trailing_write_error_count_ = 0;
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

bool InputSyncWriter::PushDataToFifo(const media::AudioBus* data,
                                     double volume,
                                     bool key_pressed,
                                     base::TimeTicks capture_time) {
  TRACE_EVENT1("audio", "InputSyncWriter::PushDataToFifo", "capture time (ms)",
               (capture_time - base::TimeTicks()).InMillisecondsF());
  if (overflow_data_.size() == kMaxOverflowBusesSize) {
    // We use |write_error_count_| for capping number of log messages.
    // |write_error_count_| also includes socket Send() errors, but those should
    // be rare.
    TRACE_EVENT_INSTANT0("audio", "InputSyncWriter::PushDataToFifo - overflow",
                         TRACE_EVENT_SCOPE_THREAD);
    if (write_error_count_ <= 50 && write_error_count_ % 10 == 0) {
      static const char* error_message = "AISW: No room in fifo.";
      LOG(WARNING) << error_message;
      log_callback_.Run(error_message);
      if (write_error_count_ == 50) {
        static const char* cap_error_message =
            "AISW: Log cap reached, suppressing further fifo overflow logs.";
        LOG(WARNING) << cap_error_message;
        log_callback_.Run(error_message);
      }
    }
    return false;
  }

  if (overflow_data_.empty()) {
    static const char* message = "AISW: Starting to use fifo.";
    log_callback_.Run(message);
  }

  // Push data to fifo.
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(data->channels(), data->frames());
  data->CopyTo(audio_bus.get());
  overflow_data_.emplace_back(volume, key_pressed, capture_time,
                              std::move(audio_bus));
  DCHECK_LE(overflow_data_.size(), static_cast<size_t>(kMaxOverflowBusesSize));

  return true;
}

bool InputSyncWriter::WriteDataFromFifoToSharedMemory() {
  TRACE_EVENT0("audio", "InputSyncWriter::WriteDataFromFifoToSharedMemory");
  if (overflow_data_.empty())
    return true;

  const size_t segment_count = audio_buses_.size();
  bool write_error = false;
  auto data_it = overflow_data_.begin();

  while (data_it != overflow_data_.end() &&
         number_of_filled_segments_ < segment_count) {
    // Write parameters to shared memory.
    WriteParametersToCurrentSegment(data_it->volume_, data_it->key_pressed_,
                                    data_it->capture_time_);

    // Copy data from the fifo into shared memory using pre-allocated audio
    // buses.
    data_it->audio_bus_->CopyTo(audio_buses_[current_segment_id_].get());

    if (!SignalDataWrittenAndUpdateCounters())
      write_error = true;

    ++data_it;
  }

  // Erase all copied data from fifo.
  overflow_data_.erase(overflow_data_.begin(), data_it);

  if (overflow_data_.empty()) {
    static const char* message = "AISW: Fifo emptied.";
    log_callback_.Run(message);
  }

  return !write_error;
}

void InputSyncWriter::WriteParametersToCurrentSegment(
    double volume,
    bool key_pressed,
    base::TimeTicks capture_time) {
  TRACE_EVENT1("audio", "WriteParametersToCurrentSegment", "capture time (ms)",
               (capture_time - base::TimeTicks()).InMillisecondsF());
  uint8_t* ptr = static_cast<uint8_t*>(shared_memory_mapping_.memory());
  CHECK_LT(current_segment_id_, audio_buses_.size());
  ptr += current_segment_id_ * shared_memory_segment_size_;
  auto* buffer = reinterpret_cast<media::AudioInputBuffer*>(ptr);
  buffer->params.volume = volume;
  buffer->params.size = audio_bus_memory_size_;
  buffer->params.key_pressed = key_pressed;
  buffer->params.capture_time_us =
      (capture_time - base::TimeTicks()).InMicroseconds();
  buffer->params.id = next_buffer_id_;
}

bool InputSyncWriter::SignalDataWrittenAndUpdateCounters() {
  if (socket_->Send(&current_segment_id_, sizeof(current_segment_id_)) !=
      sizeof(current_segment_id_)) {
    // Ensure we don't log consecutive errors as this can lead to a large
    // amount of logs.
    if (!had_socket_error_) {
      had_socket_error_ = true;
      static const char* error_message = "AISW: No room in socket buffer.";
      PLOG(WARNING) << error_message;
      log_callback_.Run(error_message);
      TRACE_EVENT_INSTANT0("audio", "InputSyncWriter: No room in socket buffer",
                           TRACE_EVENT_SCOPE_THREAD);
    }
    return false;
  } else {
    had_socket_error_ = false;
  }

  if (++current_segment_id_ >= audio_buses_.size())
    current_segment_id_ = 0;
  ++number_of_filled_segments_;
  CHECK_LE(number_of_filled_segments_, audio_buses_.size());
  ++next_buffer_id_;

  return true;
}

}  // namespace audio
