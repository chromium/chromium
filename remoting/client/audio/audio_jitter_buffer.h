// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_AUDIO_AUDIO_JITTER_BUFFER_H_
#define REMOTING_CLIENT_AUDIO_AUDIO_JITTER_BUFFER_H_

#include <list>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "remoting/client/audio/async_audio_data_supplier.h"
#include "remoting/client/audio/audio_stream_format.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

// This is a jitter buffer that queues up audio packets and get-data requests
// and feeds the requests with the data when the buffer has enough data.
class AudioJitterBuffer : public AsyncAudioDataSupplier {
 public:
  using OnFormatChangedCallback =
      base::RepeatingCallback<void(const AudioStreamFormat& format)>;

  // |callback| is called once the jitter buffer gets the first packet or the
  // stream format has been changed.
  // Pending get-data requests will be dropped when the stream format is
  // changed.
  explicit AudioJitterBuffer(OnFormatChangedCallback on_format_changed);

  AudioJitterBuffer(const AudioJitterBuffer&) = delete;
  AudioJitterBuffer& operator=(const AudioJitterBuffer&) = delete;

  ~AudioJitterBuffer() override;

  void AddAudioPacket(std::unique_ptr<AudioPacket> packet);

  // AsyncAudioDataSupplier implementations.
  void AsyncGetData(std::unique_ptr<GetDataRequest> request) override;
  void ClearGetDataRequests() override;

 private:
  friend class AudioJitterBufferTest;

  // Clears the jitter buffer, drops all pending requests, and notify
  // |on_format_changed_| that the format has been changed.
  void ResetBuffer(const AudioStreamFormat& new_format);

  // Feeds data from the jitter buffer into the pending requests. OnDataFilled()
  // will be called and request will be removed from the queue when a request
  // has been filled up.
  void ProcessGetDataRequests();

  // Calculates the number of bytes needed to store audio data of the given
  // duration based on |stream_format_|.
  size_t GetBufferSizeFromTime(base::TimeDelta duration) const;

  // Drops audio packets in |queued_packets_| such that the total latency
  // doesn't exceed |kMaxQueueLatency|.
  void DropOverrunPackets();

  // The stream format of the last audio packet. This is nullptr if the buffer
  // has never received any packet.
  std::unique_ptr<AudioStreamFormat> stream_format_;

  // AudioPackets queued up by the jitter buffer before they are consumed by
  // GetDataRequests.
  std::list<std::unique_ptr<AudioPacket>> queued_packets_;

  // Number of bytes that is queued in |queued_packets_|.
  size_t queued_bytes_ = 0;

  // The byte offset when reading data from the first packet of
  // |queued_packets_|. Equal to the number of bytes consumed from the first
  // packet.
  size_t first_packet_offset_ = 0;

  // Called when the stream format is changed.
  OnFormatChangedCallback on_format_changed_;

  // GetDataRequests that are not yet fulfilled.
  std::list<std::unique_ptr<GetDataRequest>> queued_requests_;

  // The buffer will not feed data to the requests if this is true.
  bool underrun_protection_mode_ = true;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_AUDIO_AUDIO_JITTER_BUFFER_H_
