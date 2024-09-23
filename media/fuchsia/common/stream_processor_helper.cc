// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/common/stream_processor_helper.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "media/base/timestamp_constants.h"

namespace media {

namespace {

// Input buffers are allocate once per decoder, the |buffer_lifetime_ordinal| is
// always the same.
constexpr uint64_t kInputBufferLifetimeOrdinal = 1;

}  // namespace

StreamProcessorHelper::IoPacket::IoPacket(size_t index,
                                          size_t offset,
                                          size_t size,
                                          base::TimeDelta timestamp,
                                          bool unit_end,
                                          bool key_frame,
                                          base::OnceClosure destroy_cb)
    : index_(index),
      offset_(offset),
      size_(size),
      timestamp_(timestamp),
      unit_end_(unit_end),
      key_frame_(key_frame) {
  destroy_callbacks_.push_front(std::move(destroy_cb));
}

StreamProcessorHelper::IoPacket::~IoPacket() {
  for (auto& cb : destroy_callbacks_) {
    std::move(cb).Run();
  }
}

StreamProcessorHelper::IoPacket::IoPacket(IoPacket&&) = default;
StreamProcessorHelper::IoPacket& StreamProcessorHelper::IoPacket::operator=(
    IoPacket&&) = default;

void StreamProcessorHelper::IoPacket::AddOnDestroyClosure(
    base::OnceClosure closure) {
  destroy_callbacks_.push_front(std::move(closure));
}

StreamProcessorHelper::StreamProcessorHelper(
    fuchsia::media::StreamProcessorPtr processor,
    Client* client)
    : processor_(std::move(processor)), client_(client), weak_factory_(this) {
  DCHECK(processor_);
  DCHECK(client_);
  weak_this_ = weak_factory_.GetWeakPtr();

  processor_.set_error_handler(
      [this](zx_status_t status) {
        ZX_LOG(ERROR, status)
            << "The fuchsia.media.StreamProcessor channel was terminated.";
        OnError();
      });

  processor_.events().OnStreamFailed =
      fit::bind_member(this, &StreamProcessorHelper::OnStreamFailed);
  processor_.events().OnFreeInputPacket =
      fit::bind_member(this, &StreamProcessorHelper::OnFreeInputPacket);
  processor_.events().OnInputConstraints =
      fit::bind_member(this, &StreamProcessorHelper::OnInputConstraints);
  processor_.events().OnOutputConstraints =
      fit::bind_member(this, &StreamProcessorHelper::OnOutputConstraints);
  processor_.events().OnOutputFormat =
      fit::bind_member(this, &StreamProcessorHelper::OnOutputFormat);
  processor_.events().OnOutputPacket =
      fit::bind_member(this, &StreamProcessorHelper::OnOutputPacket);
  processor_.events().OnOutputEndOfStream =
      fit::bind_member(this, &StreamProcessorHelper::OnOutputEndOfStream);

  processor_->EnableOnStreamFailed();
}

StreamProcessorHelper::~StreamProcessorHelper() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void StreamProcessorHelper::Process(IoPacket input) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(processor_);

  fuchsia::media::Packet packet;
  packet.mutable_header()->set_buffer_lifetime_ordinal(
      kInputBufferLifetimeOrdinal);
  packet.mutable_header()->set_packet_index(input.buffer_index());
  packet.set_buffer_index(packet.header().packet_index());
  packet.set_timestamp_ish(input.timestamp().InNanoseconds());
  packet.set_stream_lifetime_ordinal(stream_lifetime_ordinal_);
  packet.set_start_offset(input.offset());
  packet.set_valid_length_bytes(input.size());
  packet.set_known_end_access_unit(input.unit_end());

  active_stream_ = true;

  if (!input.format().IsEmpty()) {
    processor_->QueueInputFormatDetails(stream_lifetime_ordinal_,
                                        fidl::Clone(input.format()));
  }

  DCHECK(!input_packets_.contains(input.buffer_index()));
  input_packets_.insert_or_assign(input.buffer_index(), std::move(input));
  processor_->QueueInputPacket(std::move(packet));
}

void StreamProcessorHelper::ProcessEos() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(processor_);

  active_stream_ = true;
  processor_->QueueInputEndOfStream(stream_lifetime_ordinal_);
  processor_->FlushEndOfStreamAndCloseStream(stream_lifetime_ordinal_);
}

void StreamProcessorHelper::Reset() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!active_stream_) {
    // Nothing to do if we don't have an active stream.
    return;
  }

  if (processor_) {
    processor_->CloseCurrentStream(stream_lifetime_ordinal_,
                                   /*release_input_buffers=*/false,
                                   /*release_output_buffers=*/false);
  }

  stream_lifetime_ordinal_ += 2;
  active_stream_ = false;
}

void StreamProcessorHelper::OnStreamFailed(uint64_t stream_lifetime_ordinal,
                                           fuchsia::media::StreamError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (stream_lifetime_ordinal_ != stream_lifetime_ordinal) {
    return;
  }

  if (error == fuchsia::media::StreamError::DECRYPTOR_NO_KEY) {
    // Always reset the stream since the current one has failed.
    Reset();

    client_->OnStreamProcessorNoKey();
    return;
  }

  OnError();
}

void StreamProcessorHelper::OnFreeInputPacket(
    fuchsia::media::PacketHeader free_input_packet) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!free_input_packet.has_packet_index()) {
    DLOG(ERROR) << "Received OnFreeInputPacket() with missing required fields.";
    OnError();
    return;
  }

  auto it = input_packets_.find(free_input_packet.packet_index());
  if (it == input_packets_.end()) {
    DLOG(ERROR) << "Received OnFreeInputPacket() with invalid packet index.";
    OnError();
    return;
  }

  // The packet should be destroyed only after it's removed from
  // |input_packets_|. Otherwise packet destruction observer may call Process()
  // for the next packet while the current packet is still in |input_packets_|.
  auto packet = std::move(it->second);
  input_packets_.erase(it);
}

void StreamProcessorHelper::OnInputConstraints(
    fuchsia::media::StreamBufferConstraints input_constraints) {
  if (!input_constraints.has_buffer_constraints_version_ordinal()) {
    DLOG(ERROR)
        << "Received OnInputConstraints() with missing required fields.";
    OnError();
    return;
  }

  if (input_constraints.buffer_constraints_version_ordinal() !=
      kInputBufferLifetimeOrdinal) {
    DLOG(ERROR) << "Received OnInputConstraints() unexpected version ordinal: "
                << input_constraints.buffer_constraints_version_ordinal();
    OnError();
    return;
  }

  client_->OnStreamProcessorAllocateInputBuffers(input_constraints);
}

void StreamProcessorHelper::OnOutputConstraints(
    fuchsia::media::StreamOutputConstraints output_constraints) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!output_constraints.has_stream_lifetime_ordinal()) {
    DLOG(ERROR)
        << "Received OnOutputConstraints() with missing required fields.";
    OnError();
    return;
  }

  if (output_constraints.stream_lifetime_ordinal() !=
      stream_lifetime_ordinal_) {
    return;
  }

  if (!output_constraints.has_buffer_constraints_action_required() ||
      !output_constraints.buffer_constraints_action_required()) {
    return;
  }

  if (!output_constraints.has_buffer_constraints()) {
    DLOG(ERROR) << "Received OnOutputConstraints() which requires buffer "
                   "constraints action, but without buffer constraints.";
    OnError();
    return;
  }

  // StreamProcessor API expects odd buffer lifetime ordinal, which is
  // incremented by 2 for each buffer generation.
  output_buffer_lifetime_ordinal_ += 2;

  output_buffer_constraints_ =
      std::move(*output_constraints.mutable_buffer_constraints());

  client_->OnStreamProcessorAllocateOutputBuffers(output_buffer_constraints_);
}

void StreamProcessorHelper::OnOutputFormat(
    fuchsia::media::StreamOutputFormat output_format) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!output_format.has_stream_lifetime_ordinal() ||
      !output_format.has_format_details()) {
    DLOG(ERROR) << "Received OnOutputFormat() with missing required fields.";
    OnError();
    return;
  }

  if (output_format.stream_lifetime_ordinal() != stream_lifetime_ordinal_) {
    return;
  }

  client_->OnStreamProcessorOutputFormat(std::move(output_format));
}

void StreamProcessorHelper::OnOutputPacket(fuchsia::media::Packet output_packet,
                                           bool error_detected_before,
                                           bool error_detected_during) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!output_packet.has_header() ||
      !output_packet.header().has_buffer_lifetime_ordinal() ||
      !output_packet.header().has_packet_index() ||
      !output_packet.has_stream_lifetime_ordinal() ||
      !output_packet.has_buffer_index()) {
    DLOG(ERROR) << "Received OnOutputPacket() with missing required fields.";
    OnError();
    return;
  }

  if (output_packet.header().buffer_lifetime_ordinal() !=
      output_buffer_lifetime_ordinal_) {
    return;
  }

  if (output_packet.stream_lifetime_ordinal() != stream_lifetime_ordinal_) {
    // Output packets from old streams still need to be recycled.
    OnRecycleOutputBuffer(output_buffer_lifetime_ordinal_,
                          output_packet.header().packet_index());
    return;
  }

  auto buffer_index = output_packet.buffer_index();
  auto packet_index = output_packet.header().packet_index();
  base::TimeDelta timestamp =
      output_packet.has_timestamp_ish()
          ? base::Nanoseconds(output_packet.timestamp_ish())
          : kNoTimestamp;

  client_->OnStreamProcessorOutputPacket(IoPacket(
      buffer_index, output_packet.start_offset(),
      output_packet.valid_length_bytes(), timestamp,
      output_packet.known_end_access_unit(),
      output_packet.has_key_frame() ? output_packet.key_frame() : false,
      base::BindOnce(&StreamProcessorHelper::OnRecycleOutputBuffer, weak_this_,
                     output_buffer_lifetime_ordinal_, packet_index)));
}

void StreamProcessorHelper::OnOutputEndOfStream(
    uint64_t stream_lifetime_ordinal,
    bool error_detected_before) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (stream_lifetime_ordinal != stream_lifetime_ordinal_) {
    return;
  }

  stream_lifetime_ordinal_ += 2;
  active_stream_ = false;

  client_->OnStreamProcessorEndOfStream();
}

void StreamProcessorHelper::OnError() {
  processor_.Unbind();
  client_->OnStreamProcessorError();
}

void StreamProcessorHelper::SetInputBufferCollectionToken(
    fuchsia::sysmem2::BufferCollectionTokenPtr sysmem_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  fuchsia::media::StreamBufferPartialSettings settings;
  settings.set_buffer_lifetime_ordinal(kInputBufferLifetimeOrdinal);
  settings.set_buffer_constraints_version_ordinal(0);
  settings.set_sysmem_token(fuchsia::sysmem::BufferCollectionTokenHandle(
      sysmem_token.Unbind().TakeChannel()));
  processor_->SetInputBufferPartialSettings(std::move(settings));
}

void StreamProcessorHelper::CompleteOutputBuffersAllocation(
    fuchsia::sysmem2::BufferCollectionTokenPtr collection_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!output_buffer_constraints_.IsEmpty());

  // Pass new output buffer settings to the stream processor.
  fuchsia::media::StreamBufferPartialSettings settings;
  settings.set_buffer_lifetime_ordinal(output_buffer_lifetime_ordinal_);
  settings.set_buffer_constraints_version_ordinal(
      output_buffer_constraints_.buffer_constraints_version_ordinal());
  settings.set_sysmem_token(fuchsia::sysmem::BufferCollectionTokenHandle(
      collection_token.Unbind().TakeChannel()));
  processor_->SetOutputBufferPartialSettings(std::move(settings));
  processor_->CompleteOutputBufferPartialSettings(
      output_buffer_lifetime_ordinal_);
}

void StreamProcessorHelper::OnRecycleOutputBuffer(
    uint64_t buffer_lifetime_ordinal,
    uint32_t packet_index) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!processor_)
    return;

  if (buffer_lifetime_ordinal != output_buffer_lifetime_ordinal_)
    return;

  fuchsia::media::PacketHeader header;
  header.set_buffer_lifetime_ordinal(buffer_lifetime_ordinal);
  header.set_packet_index(packet_index);
  processor_->RecycleOutputPacket(std::move(header));
}

}  // namespace media
