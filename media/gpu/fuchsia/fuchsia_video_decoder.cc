// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/fuchsia/fuchsia_video_decoder.h"

#include <fuchsia/mediacodec/cpp/fidl.h>
#include <zircon/rights.h>

#include "base/callback_helpers.h"
#include "base/fuchsia/component_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/libyuv/include/libyuv/video_common.h"

namespace media {

namespace {

const zx_rights_t kReadOnlyVmoRights =
    ZX_DEFAULT_VMO_RIGHTS &
    ~(ZX_RIGHT_WRITE | ZX_RIGHT_EXECUTE | ZX_RIGHT_SET_PROPERTY);

// Value passed to the codec as packet_count_for_client. It's number of output
// buffers that we expect to hold on to in the renderer.
//
// TODO(sergeyu): Figure out the right number of buffers to request. Currently
// the codec doesn't allow to reserve more than 2 client buffers, but it still
// works properly when the client holds to more than that.
const uint32_t kMaxUsedOutputFrames = 8;

zx::vmo CreateContiguousVmo(size_t size, const zx::handle& bti_handle) {
  zx::vmo vmo;
  zx_status_t status =
      zx_vmo_create_contiguous(bti_handle.get(), size, /*alignment_log2=*/0,
                               vmo.reset_and_get_address());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_vmo_create_contiguous";
    return zx::vmo();
  }

  return vmo;
}

zx::vmo CreateVmo(size_t size) {
  zx::vmo vmo;
  zx_status_t status = zx::vmo::create(size, ZX_VMO_NON_RESIZABLE, &vmo);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_vmo_create";
    return zx::vmo();
  }

  return vmo;
}

class PendingDecode {
 public:
  PendingDecode(scoped_refptr<DecoderBuffer> buffer,
                VideoDecoder::DecodeCB decode_cb)
      : buffer_(buffer), decode_cb_(decode_cb) {
    DCHECK(buffer_);
  }
  ~PendingDecode() {
    if (decode_cb_) {
      std::move(decode_cb_).Run(DecodeStatus::ABORTED);
    }
  }

  PendingDecode(PendingDecode&& other) = default;
  PendingDecode& operator=(PendingDecode&& other) = default;

  const DecoderBuffer& buffer() { return *buffer_; }

  const uint8_t* data() const { return buffer_->data() + buffer_pos_; }
  size_t bytes_left() const { return buffer_->data_size() - buffer_pos_; }
  void AdvanceCurrentPos(size_t bytes) {
    DCHECK_LE(bytes, bytes_left());
    buffer_pos_ += bytes;
  }
  VideoDecoder::DecodeCB TakeDecodeCallback() { return std::move(decode_cb_); }

 private:
  scoped_refptr<DecoderBuffer> buffer_;
  size_t buffer_pos_ = 0;
  VideoDecoder::DecodeCB decode_cb_;

  DISALLOW_COPY_AND_ASSIGN(PendingDecode);
};

class CodecBuffer {
 public:
  CodecBuffer() = default;

  bool Initialize(
      const fuchsia::mediacodec::CodecBufferConstraints& constraints) {
    size_ = constraints.per_packet_buffer_bytes_recommended;

    if (constraints.is_physically_contiguous_required) {
      vmo_ =
          CreateContiguousVmo(size_, constraints.very_temp_kludge_bti_handle);
    } else {
      vmo_ = CreateVmo(size_);
    }
    return vmo_.is_valid();
  }

  const zx::vmo& vmo() const { return vmo_; }
  size_t size() const { return size_; }

  bool ToFidlCodecBuffer(uint64_t buffer_lifetime_ordinal,
                         uint32_t buffer_index,
                         bool read_only,
                         fuchsia::mediacodec::CodecBuffer* buffer) {
    zx::vmo vmo_dup;
    zx_status_t status = vmo_.duplicate(
        read_only ? kReadOnlyVmoRights : ZX_RIGHT_SAME_RIGHTS, &vmo_dup);
    if (status != ZX_OK) {
      ZX_DLOG(ERROR, status) << "zx_handle_duplicate";
      return false;
    }

    fuchsia::mediacodec::CodecBufferDataVmo buf_data;
    buf_data.vmo_handle = std::move(vmo_dup);

    buf_data.vmo_usable_start = 0;
    buf_data.vmo_usable_size = size_;

    buffer->data.set_vmo(std::move(buf_data));
    buffer->buffer_lifetime_ordinal = buffer_lifetime_ordinal;
    buffer->buffer_index = buffer_index;

    return true;
  }

 private:
  zx::vmo vmo_;
  size_t size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CodecBuffer);
};

class InputBuffer {
 public:
  InputBuffer() = default;

  ~InputBuffer() { CallDecodeCallbackIfAny(DecodeStatus::ABORTED); }

  InputBuffer(InputBuffer&& other) = default;
  InputBuffer& operator=(InputBuffer&& other) = default;

  bool Initialize(
      const fuchsia::mediacodec::CodecBufferConstraints& constraints) {
    return buffer_.Initialize(constraints);
  }

  CodecBuffer& buffer() { return buffer_; }
  bool is_used() const { return is_used_; }

  // Copies as much data as possible from |pending_decode| to this input buffer.
  size_t FillFromDecodeBuffer(PendingDecode* pending_decode) {
    DCHECK(!is_used_);
    is_used_ = true;

    size_t bytes_to_fill =
        std::min(buffer_.size(), pending_decode->bytes_left());

    zx_status_t status =
        buffer_.vmo().write(pending_decode->data(), 0, bytes_to_fill);
    ZX_CHECK(status == ZX_OK, status) << "zx_vmo_write";

    pending_decode->AdvanceCurrentPos(bytes_to_fill);

    if (pending_decode->bytes_left() == 0) {
      DCHECK(!decode_cb_);
      decode_cb_ = pending_decode->TakeDecodeCallback();
    }

    return bytes_to_fill;
  }

  void CallDecodeCallbackIfAny(DecodeStatus status) {
    if (decode_cb_) {
      std::move(decode_cb_).Run(status);
    }
  }

  void OnDoneDecoding(DecodeStatus status) {
    DCHECK(is_used_);
    is_used_ = false;
    CallDecodeCallbackIfAny(status);
  }

 private:
  CodecBuffer buffer_;

  // Set to true when this buffer is being used by the codec.
  bool is_used_ = false;

  // Decode callback for the DecodeBuffer of which this InputBuffer is a part.
  // This is only set on the final InputBuffer in each DecodeBuffer.
  VideoDecoder::DecodeCB decode_cb_;

  DISALLOW_COPY_AND_ASSIGN(InputBuffer);
};

// Output buffer used to pass decoded frames from the decoder. Ref-counted
// to make it possible to share the buffers with VideoFrames, in case when a
// frame outlives the decoder.UnsafeSharedMemoryRegion
class OutputBuffer : public base::RefCountedThreadSafe<OutputBuffer> {
 public:
  OutputBuffer() = default;

  OutputBuffer(OutputBuffer&& other) = default;
  OutputBuffer& operator=(OutputBuffer&& other) = default;

  bool Initialize(
      const fuchsia::mediacodec::CodecBufferConstraints& constraints) {
    if (!buffer_.Initialize(constraints)) {
      return false;
    }

    zx_status_t status = zx::vmar::root_self()->map(
        /*vmar_offset=*/0, buffer_.vmo(), 0, buffer_.size(),
        ZX_VM_REQUIRE_NON_RESIZABLE | ZX_VM_FLAG_PERM_READ, &mapped_memory_);

    if (status != ZX_OK) {
      ZX_DLOG(ERROR, status) << "zx_vmar_map";
      mapped_memory_ = 0;
      return false;
    }

    return true;
  }

  CodecBuffer& buffer() { return buffer_; }

  const uint8_t* mapped_memory() {
    DCHECK(mapped_memory_);
    return reinterpret_cast<uint8_t*>(mapped_memory_);
  }

 private:
  friend class RefCountedThreadSafe<OutputBuffer>;

  ~OutputBuffer() {
    if (mapped_memory_) {
      zx_status_t status =
          zx::vmar::root_self()->unmap(mapped_memory_, buffer_.size());
      if (status != ZX_OK) {
        ZX_LOG(FATAL, status) << "zx_vmar_unmap";
      }
    }
  }

  CodecBuffer buffer_;

  uintptr_t mapped_memory_ = 0;

  DISALLOW_COPY_AND_ASSIGN(OutputBuffer);
};

}  // namespace

class FuchsiaVideoDecoder : public VideoDecoder {
 public:
  FuchsiaVideoDecoder();
  ~FuchsiaVideoDecoder() override;

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(
      const VideoDecoderConfig& config,
      bool low_delay,
      CdmContext* cdm_context,
      const InitCB& init_cb,
      const OutputCB& output_cb,
      const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& closure) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;

 private:
  // Event handlers for |codec_|.
  void OnStreamFailed(uint64_t stream_lifetime_ordinal);
  void OnInputConstraints(
      fuchsia::mediacodec::CodecBufferConstraints input_constraints);
  void OnFreeInputPacket(
      fuchsia::mediacodec::CodecPacketHeader free_input_packet);
  void OnOutputConfig(fuchsia::mediacodec::CodecOutputConfig output_config);
  void OnOutputPacket(fuchsia::mediacodec::CodecPacket output_packet,
                      bool error_detected_before,
                      bool error_detected_during);
  void OnOutputEndOfStream(uint64_t stream_lifetime_ordinal,
                           bool error_detected_before);

  void OnError();

  // Called by OnInputConstraints() to initialize input buffers.
  bool InitializeInputBuffers(
      fuchsia::mediacodec::CodecBufferConstraints constraints);

  // Pumps |pending_decodes_| to the decoder.
  void PumpInput();

  // Called by OnInputConstraints() to initialize input buffers.
  bool InitializeOutputBuffers(
      fuchsia::mediacodec::CodecBufferConstraints constraints);

  // Destruction callback for the output VideoFrame instances.
  void OnFrameDestroyed(scoped_refptr<OutputBuffer> buffer,
                        uint64_t buffer_lifetime_ordinal,
                        uint32_t packet_index);

  OutputCB output_cb_;

  // Aspect ratio specified in container, or 1.0 if it's not specified. This
  // value is used only if the aspect ratio is not specified in the bitstream.
  float container_pixel_aspect_ratio_ = 1.0;

  fuchsia::mediacodec::CodecPtr codec_;

  uint64_t stream_lifetime_ordinal_ = 1;

  // Set to true if we've sent an input packet with the current
  // stream_lifetime_ordinal_.
  bool active_stream_ = false;

  std::list<PendingDecode> pending_decodes_;
  uint64_t input_buffer_lifetime_ordinal_ = 1;
  std::vector<InputBuffer> input_buffers_;
  int num_used_input_buffers_ = 0;

  fuchsia::mediacodec::VideoUncompressedFormat output_format_;
  uint64_t output_buffer_lifetime_ordinal_ = 1;
  std::vector<scoped_refptr<OutputBuffer>> output_buffers_;
  int num_used_output_buffers_ = 0;
  int max_used_output_buffers_ = 0;

  // Non-null when flush is pending.
  VideoDecoder::DecodeCB pending_flush_cb_;

  base::WeakPtr<FuchsiaVideoDecoder> weak_this_;
  base::WeakPtrFactory<FuchsiaVideoDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FuchsiaVideoDecoder);
};

FuchsiaVideoDecoder::FuchsiaVideoDecoder() : weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

FuchsiaVideoDecoder::~FuchsiaVideoDecoder() = default;

std::string FuchsiaVideoDecoder::GetDisplayName() const {
  return "FuchsiaVideoDecoder";
}

void FuchsiaVideoDecoder::Initialize(
    const VideoDecoderConfig& config,
    bool low_delay,
    CdmContext* cdm_context,
    const InitCB& init_cb,
    const OutputCB& output_cb,
    const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb) {
  output_cb_ = output_cb;
  container_pixel_aspect_ratio_ = config.GetPixelAspectRatio();

  auto done_callback = BindToCurrentLoop(init_cb);

  fuchsia::mediacodec::CreateDecoder_Params codec_params;
  codec_params.input_details.format_details_version_ordinal = 0;

  switch (config.codec()) {
    case kCodecH264:
      codec_params.input_details.mime_type = "video/h264";
      break;
    case kCodecVP8:
      codec_params.input_details.mime_type = "video/vp8";
      break;
    case kCodecVP9:
      codec_params.input_details.mime_type = "video/vp9";
      break;
    case kCodecHEVC:
      codec_params.input_details.mime_type = "video/hevc";
      break;
    case kCodecAV1:
      codec_params.input_details.mime_type = "video/av1";
      break;

    default:
      done_callback.Run(false);
      return;
  }

  codec_params.promise_separate_access_units_on_input = true;
  codec_params.require_hw = true;

  auto codec_factory =
      base::fuchsia::ComponentContext::GetDefault()
          ->ConnectToService<fuchsia::mediacodec::CodecFactory>();
  codec_factory->CreateDecoder(std::move(codec_params), codec_.NewRequest());

  codec_.set_error_handler([this]() {
    LOG(ERROR) << "The fuchsia.mediacodec.Codec channel was terminated.";
    OnError();
  });

  codec_.events().OnStreamFailed =
      fit::bind_member(this, &FuchsiaVideoDecoder::OnStreamFailed);
  codec_.events().OnInputConstraints =
      fit::bind_member(this, &FuchsiaVideoDecoder::OnInputConstraints);
  codec_.events().OnFreeInputPacket =
      fit::bind_member(this, &FuchsiaVideoDecoder::OnFreeInputPacket);
  codec_.events().OnOutputConfig =
      fit::bind_member(this, &FuchsiaVideoDecoder::OnOutputConfig);
  codec_.events().OnOutputPacket =
      fit::bind_member(this, &FuchsiaVideoDecoder::OnOutputPacket);
  codec_.events().OnOutputEndOfStream =
      fit::bind_member(this, &FuchsiaVideoDecoder::OnOutputEndOfStream);

  codec_->EnableOnStreamFailed();

  done_callback.Run(true);
}

void FuchsiaVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                 const DecodeCB& decode_cb) {
  DCHECK_LT(static_cast<int>(pending_decodes_.size()) + num_used_input_buffers_,
            GetMaxDecodeRequests());

  if (!codec_) {
    decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  pending_decodes_.push_back(PendingDecode(buffer, decode_cb));
  PumpInput();
}

void FuchsiaVideoDecoder::Reset(const base::Closure& closure) {
  // Call DecodeCB(ABORTED) for all active decode requests.
  for (auto& buffer : input_buffers_) {
    buffer.CallDecodeCallbackIfAny(DecodeStatus::ABORTED);
  }

  // Will call DecodeCB(ABORTED) for all pending decode requests.
  pending_decodes_.clear();

  if (active_stream_) {
    codec_->CloseCurrentStream(stream_lifetime_ordinal_,
                               /*release_input_buffers=*/false,
                               /*release_output_buffers=*/false);
    stream_lifetime_ordinal_ += 2;
    active_stream_ = false;
  }

  BindToCurrentLoop(closure).Run();
}

bool FuchsiaVideoDecoder::NeedsBitstreamConversion() const {
  return true;
}

bool FuchsiaVideoDecoder::CanReadWithoutStalling() const {
  return num_used_output_buffers_ < max_used_output_buffers_;
}

int FuchsiaVideoDecoder::GetMaxDecodeRequests() const {
  // Add one extra request to be able to send new InputBuffer immediately after
  // OnFreeInputPacket().
  return input_buffers_.size() + 1;
}

void FuchsiaVideoDecoder::OnStreamFailed(uint64_t stream_lifetime_ordinal) {
  if (stream_lifetime_ordinal_ != stream_lifetime_ordinal) {
    return;
  }

  OnError();
}

void FuchsiaVideoDecoder::OnInputConstraints(
    fuchsia::mediacodec::CodecBufferConstraints input_constraints) {
  if (!InitializeInputBuffers(std::move(input_constraints))) {
    DLOG(ERROR) << "Failed to initialize input buffers.";
    OnError();
    return;
  }

  PumpInput();
}

void FuchsiaVideoDecoder::OnFreeInputPacket(
    fuchsia::mediacodec::CodecPacketHeader free_input_packet) {
  if (free_input_packet.buffer_lifetime_ordinal !=
      input_buffer_lifetime_ordinal_) {
    return;
  }

  if (free_input_packet.packet_index >= input_buffers_.size()) {
    DLOG(ERROR) << "fuchsia.mediacodec sent OnFreeInputPacket() for an unknown "
                   "packet: buffer_lifetime_ordinal="
                << free_input_packet.buffer_lifetime_ordinal
                << " packet_index=" << free_input_packet.packet_index;
    OnError();
    return;
  }

  DCHECK_GT(num_used_input_buffers_, 0);
  num_used_input_buffers_--;
  input_buffers_[free_input_packet.packet_index].OnDoneDecoding(
      DecodeStatus::OK);

  // Try to pump input in case it was blocked.
  PumpInput();
}

void FuchsiaVideoDecoder::OnOutputConfig(
    fuchsia::mediacodec::CodecOutputConfig output_config) {
  if (output_config.stream_lifetime_ordinal != stream_lifetime_ordinal_)
    return;

  auto& format = output_config.format_details;

  if (!format.domain->is_video() || !format.domain->video().is_uncompressed()) {
    DLOG(ERROR) << "Received OnOutputConfig() with invalid format.";
    OnError();
    return;
  }

  if (output_config.buffer_constraints_action_required) {
    if (!InitializeOutputBuffers(std::move(output_config.buffer_constraints))) {
      DLOG(ERROR) << "Failed to initialize output buffers.";
      OnError();
      return;
    }
  }

  output_format_ = std::move(format.domain->video().uncompressed());
}

void FuchsiaVideoDecoder::OnOutputPacket(
    fuchsia::mediacodec::CodecPacket output_packet,
    bool error_detected_before,
    bool error_detected_during) {
  VideoPixelFormat pixel_format;
  switch (output_format_.fourcc) {
    case libyuv::FOURCC_NV12:
      pixel_format = PIXEL_FORMAT_NV12;
      break;
    default:
      LOG(ERROR) << "unknown fourcc: "
                 << std::string(reinterpret_cast<char*>(&output_format_.fourcc),
                                4);
      return;
  }

  base::TimeDelta timestamp;
  if (output_packet.has_timestamp_ish)
    timestamp = base::TimeDelta::FromNanoseconds(output_packet.timestamp_ish);

  auto packet_index = output_packet.header.packet_index;
  auto& buffer = output_buffers_[packet_index];

  DCHECK_LT(num_used_output_buffers_, static_cast<int>(output_buffers_.size()));
  num_used_output_buffers_++;

  float pixel_aspect_ratio;
  if (output_format_.has_pixel_aspect_ratio) {
    pixel_aspect_ratio =
        static_cast<float>(output_format_.pixel_aspect_ratio_width) /
        static_cast<float>(output_format_.pixel_aspect_ratio_height);
  } else {
    pixel_aspect_ratio = container_pixel_aspect_ratio_;
  }

  auto display_rect = gfx::Rect(output_format_.primary_display_width_pixels,
                                output_format_.primary_display_height_pixels);

  // TODO(sergeyu): Returned frame is correct only when stride=width, which
  // is not always the case. Currently VideoFrame::WrapExternalData() doesn't
  // allow custom frame layout.
  auto frame = VideoFrame::WrapExternalData(
      pixel_format,
      gfx::Size(output_format_.primary_width_pixels,
                output_format_.primary_height_pixels),
      display_rect, GetNaturalSize(display_rect, pixel_aspect_ratio),
      const_cast<uint8_t*>(buffer->mapped_memory()) +
          output_format_.primary_start_offset,
      buffer->buffer().size() - output_format_.primary_start_offset, timestamp);

  // Pass a reference to the buffer to the destruction callback to ensure it's
  // not destroyed while the frame is being used.
  frame->AddDestructionObserver(BindToCurrentLoop(
      base::BindOnce(&FuchsiaVideoDecoder::OnFrameDestroyed, weak_this_, buffer,
                     output_buffer_lifetime_ordinal_, packet_index)));

  output_cb_.Run(std::move(frame));
}

void FuchsiaVideoDecoder::OnOutputEndOfStream(uint64_t stream_lifetime_ordinal,
                                              bool error_detected_before) {
  if (stream_lifetime_ordinal != stream_lifetime_ordinal_) {
    return;
  }

  stream_lifetime_ordinal_ += 2;
  active_stream_ = false;

  std::move(pending_flush_cb_).Run(DecodeStatus::OK);
}

void FuchsiaVideoDecoder::OnError() {
  codec_.Unbind();

  auto weak_this = weak_this_;

  // Call decode callback with DECODE_ERROR before clearing input_buffers_.
  // Otherwise InputBuffer destructor would call it with ABORTED.
  for (auto& buffer : input_buffers_) {
    if (buffer.is_used()) {
      buffer.OnDoneDecoding(DecodeStatus::DECODE_ERROR);

      // DecodeCB(DECODE_ERROR) may destroy |this|.
      if (!weak_this) {
        return;
      }
    }
  }

  // Will call DecodeCB(ABORTED) for all pending decode requests.
  pending_decodes_.clear();

  num_used_input_buffers_ = 0;
  input_buffers_.clear();

  num_used_output_buffers_ = 0;
  output_buffers_.clear();
}

bool FuchsiaVideoDecoder::InitializeInputBuffers(
    fuchsia::mediacodec::CodecBufferConstraints constraints) {
  input_buffer_lifetime_ordinal_ += 2;

  auto settings = constraints.default_settings;
  settings.buffer_lifetime_ordinal = input_buffer_lifetime_ordinal_;
  settings.packet_count_for_client = 0;
  codec_->SetInputBufferSettings(settings);

  int total_buffers =
      settings.packet_count_for_codec + settings.packet_count_for_client;
  std::vector<InputBuffer> new_buffers(total_buffers);

  for (int i = 0; i < total_buffers; ++i) {
    fuchsia::mediacodec::CodecBuffer codec_buffer;

    if (!new_buffers[i].Initialize(constraints) ||
        !new_buffers[i].buffer().ToFidlCodecBuffer(
            input_buffer_lifetime_ordinal_, i, /*read_only=*/true,
            &codec_buffer)) {
      return false;
    }

    codec_->AddInputBuffer(std::move(codec_buffer));
  }

  num_used_input_buffers_ = 0;
  input_buffers_ = std::move(new_buffers);

  return true;
}

void FuchsiaVideoDecoder::PumpInput() {
  // Nothing to do if a codec error has occurred or input buffers have not been
  // initialized (which happens in response to OnInputConstraints() event).
  if (!codec_ || input_buffers_.empty())
    return;

  while (!pending_decodes_.empty()) {
    // Decode() is not supposed to be called while Decode(EOS) is pending.
    DCHECK(!pending_flush_cb_);

    if (pending_decodes_.front().buffer().end_of_stream()) {
      active_stream_ = true;
      codec_->QueueInputEndOfStream(stream_lifetime_ordinal_);
      codec_->FlushEndOfStreamAndCloseStream(stream_lifetime_ordinal_);
      pending_flush_cb_ = pending_decodes_.front().TakeDecodeCallback();
      pending_decodes_.pop_front();
      continue;
    }

    DCHECK_LE(num_used_input_buffers_, static_cast<int>(input_buffers_.size()));
    if (num_used_input_buffers_ == static_cast<int>(input_buffers_.size())) {
      // No input buffer available.
      return;
    }

    auto input_buffer =
        std::find_if(input_buffers_.begin(), input_buffers_.end(),
                     [](const InputBuffer& buf) { return !buf.is_used(); });
    CHECK(input_buffer != input_buffers_.end());

    num_used_input_buffers_++;
    size_t bytes_filled =
        input_buffer->FillFromDecodeBuffer(&pending_decodes_.front());

    fuchsia::mediacodec::CodecPacket packet;
    packet.header.buffer_lifetime_ordinal = input_buffer_lifetime_ordinal_;
    packet.header.packet_index = input_buffer - input_buffers_.begin();
    packet.has_timestamp_ish = true;
    packet.timestamp_ish =
        pending_decodes_.front().buffer().timestamp().InNanoseconds();
    packet.stream_lifetime_ordinal = stream_lifetime_ordinal_;
    packet.start_offset = 0;
    packet.valid_length_bytes = bytes_filled;

    active_stream_ = true;
    codec_->QueueInputPacket(std::move(packet));

    if (pending_decodes_.front().bytes_left() == 0) {
      pending_decodes_.pop_front();
    }
  }
}

bool FuchsiaVideoDecoder::InitializeOutputBuffers(
    fuchsia::mediacodec::CodecBufferConstraints constraints) {
  // mediacodec API expects odd buffer lifetime ordinal, which is incremented by
  // 2 for each buffer generation.
  output_buffer_lifetime_ordinal_ += 2;

  auto settings = constraints.default_settings;
  settings.buffer_lifetime_ordinal = output_buffer_lifetime_ordinal_;

  max_used_output_buffers_ =
      std::min(kMaxUsedOutputFrames, constraints.packet_count_for_client_max);
  settings.packet_count_for_client = max_used_output_buffers_;

  codec_->SetOutputBufferSettings(std::move(settings));

  int total_buffers =
      settings.packet_count_for_codec + settings.packet_count_for_client;
  std::vector<scoped_refptr<OutputBuffer>> new_buffers(total_buffers);

  for (int i = 0; i < total_buffers; ++i) {
    fuchsia::mediacodec::CodecBuffer codec_buffer;
    new_buffers[i] = new OutputBuffer();
    if (!new_buffers[i]->Initialize(constraints) ||
        !new_buffers[i]->buffer().ToFidlCodecBuffer(
            output_buffer_lifetime_ordinal_, i, /*read_only=*/false,
            &codec_buffer)) {
      return false;
    }

    codec_->AddOutputBuffer(std::move(codec_buffer));
  }

  num_used_output_buffers_ = 0;
  output_buffers_ = std::move(new_buffers);

  return true;
}

void FuchsiaVideoDecoder::OnFrameDestroyed(scoped_refptr<OutputBuffer> buffer,
                                           uint64_t buffer_lifetime_ordinal,
                                           uint32_t packet_index) {
  if (!codec_)
    return;

  if (buffer_lifetime_ordinal == output_buffer_lifetime_ordinal_) {
    DCHECK_GT(num_used_output_buffers_, 0);
    num_used_output_buffers_--;
    codec_->RecycleOutputPacket(fuchsia::mediacodec::CodecPacketHeader{
        buffer_lifetime_ordinal, packet_index});
  }
}

std::unique_ptr<VideoDecoder> CreateFuchsiaVideoDecoder() {
  return std::make_unique<FuchsiaVideoDecoder>();
}

}  // namespace media
