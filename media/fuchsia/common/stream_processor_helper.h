// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_COMMON_STREAM_PROCESSOR_HELPER_H_
#define MEDIA_FUCHSIA_COMMON_STREAM_PROCESSOR_HELPER_H_

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/sysmem/cpp/fidl.h>

#include <forward_list>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

// Helper class of fuchsia::media::StreamProcessor. It's responsible for:
// 1. Data validation check.
// 2. Stream/Buffer life time management.
// 3. Configure StreamProcessor and input/output buffer settings.
class MEDIA_EXPORT StreamProcessorHelper {
 public:
  class MEDIA_EXPORT IoPacket {
   public:
    IoPacket(size_t index,
             size_t offset,
             size_t size,
             base::TimeDelta timestamp,
             bool unit_end,
             bool key_frame,
             base::OnceClosure destroy_cb);

    IoPacket(const IoPacket&) = delete;
    IoPacket& operator=(const IoPacket&) = delete;

    ~IoPacket();

    IoPacket(IoPacket&&);
    IoPacket& operator=(IoPacket&&);

    size_t buffer_index() const { return index_; }
    size_t offset() const { return offset_; }
    size_t size() const { return size_; }
    base::TimeDelta timestamp() const { return timestamp_; }
    bool unit_end() const { return unit_end_; }
    bool key_frame() const { return key_frame_; }
    const fuchsia::media::FormatDetails& format() const { return format_; }
    void set_format(fuchsia::media::FormatDetails format) {
      format_ = std::move(format);
    }

    // Adds a |closure| that will be called when the packet is destroyed.
    void AddOnDestroyClosure(base::OnceClosure closure);

   private:
    size_t index_;
    size_t offset_;
    size_t size_;
    base::TimeDelta timestamp_;
    bool unit_end_;
    bool key_frame_;
    fuchsia::media::FormatDetails format_;
    std::forward_list<base::OnceClosure> destroy_callbacks_;
  };

  class Client {
   public:
    // Allocate input buffers with the given constraints. Clients should call
    // SetInputBufferCollectionToken to finish the buffer allocation flow.
    // Implementing this method is optional if a client chooses to allocate
    // input buffers before input constraints are returned from the
    // StreamProcessor.
    virtual void OnStreamProcessorAllocateInputBuffers(
        const fuchsia::media::StreamBufferConstraints& stream_constraints) {}

    // Allocate output buffers with the given constraints. Client should call
    // CompleteOutputBuffersAllocation to finish the buffer allocation flow.
    virtual void OnStreamProcessorAllocateOutputBuffers(
        const fuchsia::media::StreamBufferConstraints& stream_constraints) = 0;

    // Called when all the pushed packets are processed.
    virtual void OnStreamProcessorEndOfStream() = 0;

    // Called when output format is available.
    virtual void OnStreamProcessorOutputFormat(
        fuchsia::media::StreamOutputFormat format) = 0;

    // Called when output packet is available. Deleting |packet| will notify
    // StreamProcessor the output buffer is available to be re-used. Client
    // should delete |packet| on the same thread as this function.
    virtual void OnStreamProcessorOutputPacket(IoPacket packet) = 0;

    // Only available for decryption, which indicates currently the
    // StreamProcessor doesn't have the content key to process.
    virtual void OnStreamProcessorNoKey() = 0;

    // Called when any fatal errors happens.
    virtual void OnStreamProcessorError() = 0;

   protected:
    virtual ~Client() = default;
  };

  StreamProcessorHelper(fuchsia::media::StreamProcessorPtr processor,
                        Client* client);

  StreamProcessorHelper(const StreamProcessorHelper&) = delete;
  StreamProcessorHelper& operator=(const StreamProcessorHelper&) = delete;

  ~StreamProcessorHelper();

  // Process one packet. Caller can reuse the underlying buffer when the
  // |packet| is destroyed.
  void Process(IoPacket packet);

  // Push End-Of-Stream to StreamProcessor. No more data should be sent to
  // StreamProcessor without calling Reset.
  void ProcessEos();

  // Sets buffer collection tocken to use for input buffers.
  void SetInputBufferCollectionToken(
      fuchsia::sysmem2::BufferCollectionTokenPtr token);

  // Provide output BufferCollectionToken to finish StreamProcessor buffer
  // setup flow. Should be called only after AllocateOutputBuffers.
  void CompleteOutputBuffersAllocation(
      fuchsia::sysmem2::BufferCollectionTokenPtr token);

  // Closes the current stream and starts a new one. After that all packets
  // passed to Process() will be sent with a new |stream_lifetime_ordinal|
  // value.
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
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_COMMON_STREAM_PROCESSOR_HELPER_H_
