// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_INPUT_SYNC_WRITER_H_
#define SERVICES_AUDIO_INPUT_SYNC_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/sync_socket.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "input_glitch_counter.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"
#include "services/audio/input_controller.h"

namespace audio {

// A InputController::SyncWriter implementation using SyncSocket. This
// is used by InputController to provide a low latency data source for
// transmitting audio packets between the browser process and the renderer
// process.
class InputSyncWriter final : public InputController::SyncWriter {
 public:
  // Maximum fifo size (|overflow_buses_| and |overflow_params_|) in number of
  // media::AudioBuses.
  enum { kMaxOverflowBusesSize = 100 };

  InputSyncWriter() = delete;

  // Create() automatically initializes the InputSyncWriter correctly,
  // and should be strongly preferred over calling the constructor directly!
  InputSyncWriter(
      base::RepeatingCallback<void(const std::string&)> log_callback,
      base::MappedReadOnlyRegion shared_memory,
      std::unique_ptr<base::CancelableSyncSocket> socket,
      uint32_t shared_memory_segment_count,
      const media::AudioParameters& params,
      std::unique_ptr<InputGlitchCounter> glitch_counter);

  InputSyncWriter(const InputSyncWriter&) = delete;
  InputSyncWriter& operator=(const InputSyncWriter&) = delete;

  ~InputSyncWriter() final;

  static std::unique_ptr<InputSyncWriter> Create(
      base::RepeatingCallback<void(const std::string&)> log_callback,
      uint32_t shared_memory_segment_count,
      const media::AudioParameters& params,
      base::CancelableSyncSocket* foreign_socket);

  // Transfers shared memory region ownership to a caller. It shouldn't be
  // called more than once.
  base::ReadOnlySharedMemoryRegion TakeSharedMemoryRegion();

  size_t shared_memory_segment_count() const { return audio_buses_.size(); }

  // InputController::SyncWriter implementation.
  void Write(const media::AudioBus* data,
             double volume,
             bool key_pressed,
             base::TimeTicks capture_time,
             const media::AudioGlitchInfo& glitch_info) final;

  void Close() final;

 private:
  friend class InputSyncWriterTest;

  // Called by Write(). Checks the time since last called and if larger than a
  // threshold logs info about that.
  void CheckTimeSinceLastWrite();

  // Push |data| and metadata to |audio_buffer_fifo_|. Returns true if
  // successful. Logs error and returns false if the fifo already reached the
  // maximum size.
  bool PushDataToFifo(const media::AudioBus& data,
                      double volume,
                      bool key_pressed,
                      base::TimeTicks capture_time,
                      const media::AudioGlitchInfo& glitch_info);

  // Write data and audio parameters to current segment in shared memory.
  // Returns true if the data was successfully written, returns false if it was
  // dropped.
  bool WriteDataToCurrentSegment(const media::AudioBus& data,
                                 double volume,
                                 bool key_pressed,
                                 base::TimeTicks capture_time,
                                 const media::AudioGlitchInfo& glitch_info);

  // Signals over the socket that data has been written to the current segment.
  // Updates counters and returns true if successful. Logs error and returns
  // false if failure.
  bool SignalDataWrittenAndUpdateCounters();

  media::AudioInputBuffer* GetSharedInputBuffer(uint32_t segment_id) const;

  const base::RepeatingCallback<void(const std::string&)> log_callback_;

  // Socket used to signal that audio data is ready.
  const std::unique_ptr<base::CancelableSyncSocket> socket_;

  // Shared memory for audio data and associated metadata.
  base::ReadOnlySharedMemoryRegion shared_memory_region_;
  const base::WritableSharedMemoryMapping shared_memory_mapping_;

  // The size in bytes of a single audio segment in the shared memory.
  const uint32_t shared_memory_segment_size_;

  // Index of next segment to write.
  uint32_t current_segment_id_ = 0;

  // The time of the creation of this object.
  base::TimeTicks creation_time_;

  // The time of the last Write call.
  base::TimeTicks last_write_time_;

  // Size in bytes of each audio bus.
  const int audio_bus_memory_size_;

  // Increasing ID used for checking audio buffers are in correct sequence at
  // read side.
  uint32_t next_buffer_id_ = 0;

  // Next expected audio buffer index to have been read at the other side. We
  // will get the index read at the other side over the socket. Note that this
  // index does not correspond to |next_buffer_id_|, it's two separate counters.
  uint32_t next_read_buffer_index_ = 0;

  // Keeps track of number of filled buffer segments in the ring buffer to
  // ensure the we don't overwrite data that hasn't been read yet.
  size_t number_of_filled_segments_ = 0;

  // Used for logging.
  size_t fifo_full_count_ = 0;

  // Denotes that the most recent socket error has been logged. Used to avoid
  // log spam.
  bool had_socket_error_ = false;

  // Vector of audio buses allocated during construction and deleted in the
  // destructor.
  std::vector<std::unique_ptr<media::AudioBus>> audio_buses_;

  // Fifo for audio that is used in case there isn't room in the shared memory.
  // This can for example happen under load when the consumer side is starved.
  // It should ideally be rare, but we need to guarantee that the data arrives
  // since audio processing such as echo cancelling requires that to perform
  // properly.
  struct OverflowData {
    OverflowData(double volume,
                 bool key_pressed,
                 base::TimeTicks capture_time,
                 const media::AudioGlitchInfo& glitch_info,
                 std::unique_ptr<media::AudioBus> audio_bus);

    OverflowData(const OverflowData&) = delete;
    OverflowData& operator=(const OverflowData&) = delete;

    OverflowData(OverflowData&&);
    OverflowData& operator=(OverflowData&& other);

    ~OverflowData();

    double volume_;
    bool key_pressed_;
    base::TimeTicks capture_time_;
    media::AudioGlitchInfo glitch_info_;
    std::unique_ptr<media::AudioBus> audio_bus_;
  };

  std::vector<OverflowData> overflow_data_;

  std::unique_ptr<InputGlitchCounter> glitch_counter_;

  // Glitch info that has yet to be successfully communicated to the renderer.
  media::AudioGlitchInfo pending_glitch_info_;

  // Represents the glitch info of one dropped buffer.
  const media::AudioGlitchInfo dropped_buffer_glitch_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_INPUT_SYNC_WRITER_H_
