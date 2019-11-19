// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_COMMON_STREAM_PROCESSOR_HELPER_H_
#define MEDIA_FUCHSIA_COMMON_STREAM_PROCESSOR_HELPER_H_

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/sysmem/cpp/fidl.h>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

namespace media {

// Helper class of fuchsia::media::StreamProcessor. It's responsible for:
// 1. Data validation check.
// 2. Stream/Buffer life time management.
// 3. Configure StreamProcessor and input/output buffer settings.
class StreamProcessorHelper {
 public:
  class IoPacket {
   public:
    static IoPacket CreateInput(size_t index,
                                size_t size,
                                base::TimeDelta timestamp,
                                bool unit_end,
                                base::OnceClosure destroy_cb);

    static IoPacket CreateOutput(size_t index,
                                 size_t offset,
                                 size_t size,
                                 base::TimeDelta timestamp,
                                 bool unit_end,
                                 base::OnceClosure destroy_cb);

    IoPacket(size_t index,
             size_t offset,
             size_t size,
             base::TimeDelta timestamp,
             bool unit_end,
             base::OnceClosure destroy_cb);
    ~IoPacket();

    IoPacket(IoPacket&&);
    IoPacket& operator=(IoPacket&&);

    size_t index() const { return index_; }
    size_t offset() const { return offset_; }
    size_t size() const { return size_; }
    base::TimeDelta timestamp() const { return timestamp_; }
    bool unit_end() const { return unit_end_; }
    void set_format(fuchsia::media::FormatDetails format) {
      format_ = std::move(format);
    }
    const fuchsia::media::FormatDetails& format() const { return format_; }

   private:
    size_t index_;
    size_t offset_;
    size_t size_;
    base::TimeDelta timestamp_;
    bool unit_end_;
    fuchsia::media::FormatDetails format_;
    base::ScopedClosureRunner destroy_cb_;

    DISALLOW_COPY_AND_ASSIGN(IoPacket);
  };

  class Client {
   public:
    // Allocate input/output buffers with the given constraints. Client should
    // call ProvideInput/OutputBufferCollectionToken to finish the buffer
    // allocation flow.
    virtual void AllocateInputBuffers(
        const fuchsia::media::StreamBufferConstraints& stream_constraints) = 0;
    virtual void AllocateOutputBuffers(
        const fuchsia::media::StreamBufferConstraints& stream_constraints) = 0;

    // Called when all the pushed packets are processed.
    virtual void OnProcessEos() = 0;

    // Called when output format is available.
    virtual void OnOutputFormat(fuchsia::media::StreamOutputFormat format) = 0;

    // Called when output packet is available. Deleting |packet| will notify
    // StreamProcessor the output buffer is available to be re-used. Client
    // should delete |packet| on the same thread as this function.
    virtual void OnOutputPacket(IoPacket packet) = 0;

    // Only available for decryption, which indicates currently the
    // StreamProcessor doesn't have the content key to process.
    virtual void OnNoKey() = 0;

    // Called when any fatal errors happens.
    virtual void OnError() = 0;

   protected:
    virtual ~Client() = default;
  };

  StreamProcessorHelper(fuchsia::media::StreamProcessorPtr processor,
                        Client* client);
  ~StreamProcessorHelper();

  // Process one packet. Caller can reuse the underlying buffer when the
  // |packet| is destroyed.
  void Process(IoPacket packet);

  // Push End-Of-Stream to StreamProcessor. No more data should be sent to
  // StreamProcessor without calling Reset.
  void ProcessEos();

  // Provide input/output BufferCollectionToken to finish StreamProcessor buffer
  // setup flow.
  void CompleteInputBuffersAllocation(
      fuchsia::sysmem::BufferCollectionTokenPtr token);
  void CompleteOutputBuffersAllocation(
      size_t num_buffers_for_client,
      size_t num_buffers_for_server,
      fuchsia::sysmem::BufferCollectionTokenPtr token);

  void Reset();

 private:
  // Event handlers for |processor_|.
  void OnStreamFailed(uint64_t stream_lifetime_ordinal,
                      fuchsia::media::StreamError error);
  void OnInputConstraints(
      fuchsia::media::StreamBufferConstraints input_constraints);
  void OnFreeInputPacket(fuchsia::media::PacketHeader free_input_packet);
  void OnOutputConstraints(
      fuchsia::media::StreamOutputConstraints output_constraints);
  void OnOutputFormat(fuchsia::media::StreamOutputFormat output_format);
  void OnOutputPacket(fuchsia::media::Packet output_packet,
                      bool error_detected_before,
                      bool error_detected_during);
  void OnOutputEndOfStream(uint64_t stream_lifetime_ordinal,
                           bool error_detected_before);

  void OnError();

  void OnRecycleOutputBuffer(uint64_t buffer_lifetime_ordinal,
                             uint32_t packet_index);

  uint64_t stream_lifetime_ordinal_ = 1;

  // Set to true if we've sent an input packet with the current
  // stream_lifetime_ordinal_.
  bool active_stream_ = false;

  // Input buffers.
  uint64_t input_buffer_lifetime_ordinal_ = 1;
  fuchsia::media::StreamBufferConstraints input_buffer_constraints_;

  // Map from packet index to corresponding input IoPacket. IoPacket should be
  // owned by this class until StreamProcessor released the buffer.
  base::flat_map<size_t, IoPacket> input_packets_;

  // Output buffers.
  uint64_t output_buffer_lifetime_ordinal_ = 1;
  fuchsia::media::StreamBufferConstraints output_buffer_constraints_;

  fuchsia::media::StreamProcessorPtr processor_;
  Client* const client_;

  // FIDL interfaces are thread-affine (see crbug.com/1012875).
  THREAD_CHECKER(thread_checker_);

  base::WeakPtr<StreamProcessorHelper> weak_this_;
  base::WeakPtrFactory<StreamProcessorHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StreamProcessorHelper);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_COMMON_STREAM_PROCESSOR_HELPER_H_
