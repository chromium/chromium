// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/mac/video_toolbox_av1_accelerator.h"

#include "base/numerics/safe_conversions.h"
#include "media/base/media_log.h"
#include "media/base/video_types.h"
#include "media/gpu/mac/vt_config_util.h"
#include "third_party/libgav1/src/src/obu_parser.h"

namespace media {

VideoToolboxAV1Accelerator::VideoToolboxAV1Accelerator(
    std::unique_ptr<MediaLog> media_log,
    std::optional<gfx::HDRMetadata> hdr_metadata,
    DecodeCB decode_cb,
    OutputCB output_cb)
    : media_log_(std::move(media_log)),
      hdr_metadata_(std::move(hdr_metadata)),
      decode_cb_(std::move(decode_cb)),
      output_cb_(std::move(output_cb)) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

VideoToolboxAV1Accelerator::~VideoToolboxAV1Accelerator() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

AV1Decoder::AV1Accelerator::Status VideoToolboxAV1Accelerator::SetStream(
    base::span<const uint8_t> stream,
    const DecryptConfig* decrypt_config) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  temporal_unit_data_.reset();
  format_processed_ = false;

  // Create block buffer.
  OSStatus status = CMBlockBufferCreateEmpty(
      /*structure_allocator=*/kCFAllocatorDefault,
      /*sub_block_capacity=*/0,
      /*flags=*/0, temporal_unit_data_.InitializeInto());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMBlockBufferCreateWithMemoryBlock()";
    return Status::kFail;
  }

  // Allocate memory.
  status =
      CMBlockBufferAppendMemoryBlock(/*the_buffer=*/temporal_unit_data_.get(),
                                     /*memory_block=*/nullptr,
                                     /*block_length=*/stream.size(),
                                     /*block_allocator=*/kCFAllocatorDefault,
                                     /*custom_block_source=*/nullptr,
                                     /*offset_to_data=*/0,
                                     /*data_length=*/stream.size(),
                                     /*flags=*/0);
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMBlockBufferAppendMemoryBlock()";
    return Status::kFail;
  }

  status = CMBlockBufferAssureBlockMemory(temporal_unit_data_.get());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMBlockBufferAssureBlockMemory()";
    return Status::kFail;
  }

  // Copy the data.
  status = CMBlockBufferReplaceDataBytes(
      /*sourceBytes=*/stream.data(),
      /*destinationBuffer=*/temporal_unit_data_.get(),
      /*offsetIntoDestination=*/0,
      /*dataLength=*/stream.size());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMBlockBufferReplaceDataBytes()";
    return Status::kFail;
  }

  return Status::kOk;
}

scoped_refptr<AV1Picture> VideoToolboxAV1Accelerator::CreateAV1Picture(
    bool apply_grain) {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::MakeRefCounted<AV1Picture>();
}

VideoToolboxAV1Accelerator::Status VideoToolboxAV1Accelerator::SubmitDecode(
    const AV1Picture& pic,
    const libgav1::ObuSequenceHeader& sequence_header,
    const AV1ReferenceFrameVector& ref_frames,
    const libgav1::Vector<libgav1::TileBuffer>& tile_buffers,
    base::span<const uint8_t> data) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // It's only necessary to process format the once per temporal unit.
  if (format_processed_) {
    return Status::kOk;
  }

  // Update `active_format_`.
  if (!ProcessFormat(pic, sequence_header, data)) {
    return Status::kFail;
  }

  format_processed_ = true;

  return Status::kOk;
}

bool VideoToolboxAV1Accelerator::OutputPicture(const AV1Picture& pic) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!temporal_unit_data_ || !active_format_) {
    return false;
  }

  // Wrap the temporal unit in a sample.
  size_t data_size = CMBlockBufferGetDataLength(temporal_unit_data_.get());
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample;
  OSStatus status = CMSampleBufferCreate(
      /*allocator=*/kCFAllocatorDefault,
      /*dataBuffer=*/temporal_unit_data_.get(),
      /*dataReady=*/true,
      /*makeDataReadyCallback=*/nullptr,
      /*makeDataReadyRefcon=*/nullptr,
      /*formatDescription=*/active_format_.get(),
      /*numSamples=*/1,
      /*numSampleTimingEntries=*/0,
      /*sampleTimingArray=*/nullptr,
      /*numSampleSizeEntries=*/1,
      /*sampleSizeArray=*/&data_size, sample.InitializeInto());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMSampleBufferCreate()";
    return false;
  }

  // Submit for decoding.
  // TODO(crbug.com/40227557): Replace all const ref AV1Picture with
  // scoped_refptr.
  decode_cb_.Run(std::move(sample), session_metadata_,
                 base::WrapRefCounted(const_cast<AV1Picture*>(&pic)));

  // Schedule output.
  output_cb_.Run(base::WrapRefCounted(const_cast<AV1Picture*>(&pic)));

  temporal_unit_data_.reset();

  return true;
}

bool VideoToolboxAV1Accelerator::ProcessFormat(
    const AV1Picture& pic,
    const libgav1::ObuSequenceHeader& sequence_header,
    base::span<const uint8_t> data) {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/40227557): Consider merging with CreateFormatExtensions() to
  // avoid converting back and forth.
  VideoColorSpace color_space = pic.get_colorspace();

  VideoCodecProfile profile;
  switch (sequence_header.profile) {
    case libgav1::kProfile0:
      profile = AV1PROFILE_PROFILE_MAIN;
      break;
    case libgav1::kProfile1:
      profile = AV1PROFILE_PROFILE_HIGH;
      break;
    case libgav1::kProfile2:
      profile = AV1PROFILE_PROFILE_PRO;
      break;
    default:
      profile = VIDEO_CODEC_PROFILE_UNKNOWN;
      break;
  }

  std::optional<gfx::HDRMetadata> hdr_metadata = pic.hdr_metadata();
  if (!hdr_metadata) {
    hdr_metadata = hdr_metadata_;
  }

  // TODO(crbug.com/40936765): Should this be the current frame size, or the
  // sequence max frame size?
  gfx::Size coded_size(base::strict_cast<int>(pic.frame_header.width),
                       base::strict_cast<int>(pic.frame_header.height));

  // If the parameters have changed, generate a new format.
  if (color_space != active_color_space_ || profile != active_profile_ ||
      hdr_metadata != active_hdr_metadata_ ||
      coded_size != active_coded_size_) {
    active_format_.reset();

    // Generate the av1c.
    size_t av1c_size = 0;
    std::unique_ptr<uint8_t[]> av1c =
        libgav1::ObuParser::GetAV1CodecConfigurationBox(
            data.data(), data.size(), &av1c_size);
    base::span<const uint8_t> av1c_span =
        base::make_span(av1c.get(), av1c_size);

    // Build a format configuration with AV1 extensions.
    base::apple::ScopedCFTypeRef<CFDictionaryRef> format_config =
        CreateFormatExtensions(kCMVideoCodecType_AV1, profile,
                               sequence_header.color_config.bitdepth,
                               color_space, hdr_metadata, av1c_span);
    if (!format_config) {
      MEDIA_LOG(ERROR, media_log_.get())
          << "Failed to create format extensions";
      return false;
    }

    // Create the format description.
    base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> format;
    OSStatus status = CMVideoFormatDescriptionCreate(
        /*allocator=*/kCFAllocatorDefault,
        /*codecType=*/kCMVideoCodecType_AV1,
        /*width=*/coded_size.width(),
        /*height=*/coded_size.height(),
        /*extensions=*/format_config.get(), active_format_.InitializeInto());
    if (status != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
          << "CMVideoFormatDescriptionCreate()";
      return false;
    }

    // Save the configuration for later comparison.
    active_color_space_ = color_space;
    active_profile_ = profile;
    active_hdr_metadata_ = hdr_metadata;
    active_coded_size_ = coded_size;

    // Update session configuration.
    session_metadata_ = VideoToolboxDecompressionSessionMetadata{
        /*allow_software_decoding=*/false,
        /*bit_depth=*/
        base::checked_cast<uint8_t>(sequence_header.color_config.bitdepth),
        /*chroma_sampling=*/VideoChromaSampling::k420,
        /*has_alpha=*/false,
        /*visible_rect=*/pic.visible_rect()};
  }

  return true;
}

}  // namespace media
