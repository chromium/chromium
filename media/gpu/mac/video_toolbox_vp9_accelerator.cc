// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_vp9_accelerator.h"

#include <utility>
#include <vector>

#include "base/notreached.h"
#include "media/base/media_log.h"
#include "media/gpu/mac/vt_config_util.h"

namespace media {

VideoToolboxVP9Accelerator::VideoToolboxVP9Accelerator(
    std::unique_ptr<MediaLog> media_log,
    absl::optional<gfx::HDRMetadata> hdr_metadata,
    DecodeCB decode_cb,
    OutputCB output_cb)
    : media_log_(std::move(media_log)),
      hdr_metadata_(std::move(hdr_metadata)),
      decode_cb_(std::move(decode_cb)),
      output_cb_(std::move(output_cb)) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

VideoToolboxVP9Accelerator::~VideoToolboxVP9Accelerator() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<VP9Picture> VideoToolboxVP9Accelerator::CreateVP9Picture() {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::MakeRefCounted<VP9Picture>();
}

VideoToolboxVP9Accelerator::Status VideoToolboxVP9Accelerator::SubmitDecode(
    scoped_refptr<VP9Picture> pic,
    const Vp9SegmentationParams& segm_params,
    const Vp9LoopFilterParams& lf_params,
    const Vp9ReferenceFrameVector& reference_frames,
    const base::OnceClosure done_cb) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // No callback because SupportsContextProbabilityReadback() returns false.
  DCHECK(!done_cb);
  // `show_existing_frame` pictures go directly to OutputPicture().
  DCHECK(!pic->frame_hdr->show_existing_frame);

  if (!ProcessFrame(std::move(pic))) {
    return Status::kFail;
  }

  return Status::kOk;
}

bool VideoToolboxVP9Accelerator::OutputPicture(scoped_refptr<VP9Picture> pic) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pic->frame_hdr->show_frame || pic->frame_hdr->show_existing_frame);

  // `show_existing_frame` frames are not passed to SubmitDecode(), so handle
  // them as new frames.
  if (pic->frame_hdr->show_existing_frame) {
    if (!ProcessFrame(pic)) {
      return false;
    }
  }

  output_cb_.Run(std::move(pic));

  return true;
}

bool VideoToolboxVP9Accelerator::NeedsCompressedHeaderParsed() const {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

bool VideoToolboxVP9Accelerator::GetFrameContext(scoped_refptr<VP9Picture> pic,
                                                 Vp9FrameContext* frame_ctx) {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Not called because SupportsContextProbabilityReadback() returns false.
  NOTREACHED_NORETURN();
}

bool VideoToolboxVP9Accelerator::ProcessFrame(scoped_refptr<VP9Picture> pic) {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Update `active_format_`.
  bool format_changed = false;
  if (!ProcessFormat(pic, &format_changed)) {
    return false;
  }

  if (format_changed && frame_data_) {
    // TODO(crbug.com/1331597): Consider dropping existing frame data. Doing so
    // probably requires handling output callbacks ourselves, so that we don't
    // have to figure out which ones are duplicates.
    // TODO(crbug.com/1331597): Add Reset() to VP9Accelerator for resetting
    // superframe state after Flush().
    MEDIA_LOG(WARNING, media_log_.get()) << "Format change inside superframe";
  }

  // If this is the first picture in the current superframe, create a buffer.
  if (!frame_data_) {
    OSStatus status =
        CMBlockBufferCreateEmpty(kCFAllocatorDefault,  // structure_allocator
                                 0,                    // sub_block_capacity
                                 0,                    // flags
                                 frame_data_.InitializeInto());
    if (status != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
          << "CMBlockBufferCreateWithMemoryBlock()";
      return false;
    }
  }

  // Append this picture to the current superframe.
  AppendData(frame_data_, pic->frame_hdr->data, pic->frame_hdr->frame_size);
  frame_sizes_.push_back(pic->frame_hdr->frame_size);

  // If this is an output picture, submit the current superframe for decoding.
  if (pic->frame_hdr->show_frame || pic->frame_hdr->show_existing_frame) {
    if (!SubmitFrames(std::move(pic))) {
      return false;
    }
  }

  return true;
}

bool VideoToolboxVP9Accelerator::ProcessFormat(scoped_refptr<VP9Picture> pic,
                                               bool* format_changed) {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/1331597): Consider merging with CreateFormatExtensions() to
  // avoid converting back and forth.
  VideoColorSpace color_space = pic->frame_hdr->GetColorSpace();

  VideoCodecProfile profile;
  switch (pic->frame_hdr->profile) {
    case 0:
      profile = VP9PROFILE_PROFILE0;
      break;
    case 1:
      profile = VP9PROFILE_PROFILE1;
      break;
    case 2:
      profile = VP9PROFILE_PROFILE2;
      break;
    case 3:
      profile = VP9PROFILE_PROFILE3;
      break;
    default:
      profile = VIDEO_CODEC_PROFILE_UNKNOWN;
      break;
  }

  absl::optional<gfx::HDRMetadata> hdr_metadata = pic->hdr_metadata();
  if (!hdr_metadata) {
    hdr_metadata = hdr_metadata_;
  }

  gfx::Size coded_size(static_cast<int>(pic->frame_hdr->frame_width),
                       static_cast<int>(pic->frame_hdr->frame_height));

  // If the parameters have changed, generate a new format.
  if (color_space != active_color_space_ || profile != active_profile_ ||
      hdr_metadata != active_hdr_metadata_ ||
      coded_size != active_coded_size_) {
    active_format_.reset();

    base::apple::ScopedCFTypeRef<CFDictionaryRef> format_config =
        CreateFormatExtensions(kCMVideoCodecType_VP9, profile, color_space,
                               hdr_metadata);
    if (!format_config) {
      MEDIA_LOG(ERROR, media_log_.get())
          << "Failed to create format extensions";
      return false;
    }

    base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> format;
    OSStatus status = CMVideoFormatDescriptionCreate(
        kCFAllocatorDefault, kCMVideoCodecType_VP9, coded_size.width(),
        coded_size.height(), format_config, active_format_.InitializeInto());
    if (status != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
          << "CMVideoFormatDescriptionCreate()";
      return false;
    }

    active_color_space_ = color_space;
    active_profile_ = profile;
    active_hdr_metadata_ = hdr_metadata;
    active_coded_size_ = coded_size;

    session_metadata_ = VideoToolboxSessionMetadata{
        /*allow_software_decoding=*/false,
        /*is_hbd=*/pic->frame_hdr->bit_depth > 8,
    };

    *format_changed = true;
  } else {
    *format_changed = false;
  }

  return true;
}

bool VideoToolboxVP9Accelerator::SubmitFrames(
    scoped_refptr<VP9Picture> output_pic) {
  DVLOG(4) << __func__;
  DCHECK(frame_sizes_.size() >= 1);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Take the current superframe.
  base::apple::ScopedCFTypeRef<CMBlockBufferRef> frame_data;
  std::vector<size_t> frame_sizes;
  frame_data.swap(frame_data_);
  frame_sizes.swap(frame_sizes_);

  // If there is only one frame, it can be submitted directly. Otherwise append
  // a superframe trailer. See Annex B of the VP9 specification.
  if (frame_sizes.size() > 1) {
    if (frame_sizes_.size() > 8) {
      MEDIA_LOG(ERROR, media_log_.get()) << "Too many frames in superframe";
      return false;
    }

    constexpr uint8_t kSuperframeMarker = 0b110;
    constexpr size_t kBytesPerFrameSize = 4;
    constexpr size_t kSuperframeHeaderSize = 1;

    uint8_t header = (kSuperframeMarker << 5) |
                     ((kBytesPerFrameSize - 1) << 3) | (frame_sizes.size() - 1);

    size_t trailer_size = kSuperframeHeaderSize +
                          kBytesPerFrameSize * frame_sizes.size() +
                          kSuperframeHeaderSize;

    std::vector<uint8_t> trailer;
    trailer.reserve(trailer_size);
    trailer.push_back(header);
    for (size_t frame_size : frame_sizes) {
      trailer.push_back(frame_size & 0xff);
      frame_size >>= 8;
      trailer.push_back(frame_size & 0xff);
      frame_size >>= 8;
      trailer.push_back(frame_size & 0xff);
      frame_size >>= 8;
      trailer.push_back(frame_size & 0xff);
      frame_size >>= 8;
      DCHECK(frame_size == 0);
    }
    trailer.push_back(header);
    DCHECK_EQ(trailer.size(), trailer_size);

    AppendData(frame_data, trailer.data(), trailer.size());
  }

  // Wrap the frame data in a sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample;
  size_t size = CMBlockBufferGetDataLength(frame_data);
  OSStatus status = CMSampleBufferCreate(kCFAllocatorDefault,
                                         frame_data,  // data_buffer
                                         true,        // data_ready
                                         nullptr,  // make_data_ready_callback
                                         nullptr,  // make_data_ready_refcon
                                         active_format_,  // format_description
                                         1,               // num_samples
                                         0,        // num_sample_timing_entries
                                         nullptr,  // sample_timing_array
                                         1,        // num_sample_size_entries
                                         &size,    // sample_size_array
                                         sample.InitializeInto());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMSampleBufferCreate()";
    return false;
  }

  // Submit for decoding.
  decode_cb_.Run(std::move(sample), session_metadata_, std::move(output_pic));
  return true;
}

bool VideoToolboxVP9Accelerator::AppendData(CMBlockBufferRef dest,
                                            const uint8_t* data,
                                            size_t data_size) {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t offset = CMBlockBufferGetDataLength(dest);

  // Describe the required backing memory.
  OSStatus status =
      CMBlockBufferAppendMemoryBlock(dest,                 // the_buffer
                                     nullptr,              // memory_block
                                     data_size,            // block_length
                                     kCFAllocatorDefault,  // block_allocator
                                     nullptr,    // custom_block_source
                                     0,          // offset_to_data
                                     data_size,  // data_length
                                     0);         // flags
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMBlockBufferAppendMemoryBlock()";
    return false;
  }

  // Actually allocate the backing memory.
  status = CMBlockBufferAssureBlockMemory(dest);
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMBlockBufferAssureBlockMemory()";
    return false;
  }

  // Copy the data.
  status = CMBlockBufferReplaceDataBytes(data, dest, offset, data_size);
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMBlockBufferReplaceDataBytes()";
    return false;
  }

  return true;
}

}  // namespace media
