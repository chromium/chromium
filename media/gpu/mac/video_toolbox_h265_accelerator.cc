// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_h265_accelerator.h"

#include <utility>

#include "base/sys_byteorder.h"
#include "media/base/media_log.h"

namespace media {

namespace {
constexpr size_t kNALUHeaderLength = 4;
}  // namespace

VideoToolboxH265Accelerator::VideoToolboxH265Accelerator(
    std::unique_ptr<MediaLog> media_log,
    DecodeCB decode_cb,
    OutputCB output_cb)
    : media_log_(std::move(media_log)),
      decode_cb_(std::move(decode_cb)),
      output_cb_(std::move(output_cb)) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

VideoToolboxH265Accelerator::~VideoToolboxH265Accelerator() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<H265Picture> VideoToolboxH265Accelerator::CreateH265Picture() {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::MakeRefCounted<H265Picture>();
}

void VideoToolboxH265Accelerator::ProcessVPS(
    const H265VPS* vps,
    base::span<const uint8_t> vps_nalu_data) {
  DVLOG(3) << __func__ << ": vps_video_parameter_set_id="
           << vps->vps_video_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  seen_vps_data_[vps->vps_video_parameter_set_id] =
      std::vector<uint8_t>(vps_nalu_data.begin(), vps_nalu_data.end());
}

void VideoToolboxH265Accelerator::ProcessSPS(
    const H265SPS* sps,
    base::span<const uint8_t> sps_nalu_data) {
  DVLOG(3) << __func__
           << ": sps_seq_parameter_set_id=" << sps->sps_seq_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  seen_sps_data_[sps->sps_seq_parameter_set_id] =
      std::vector<uint8_t>(sps_nalu_data.begin(), sps_nalu_data.end());
}

void VideoToolboxH265Accelerator::ProcessPPS(
    const H265PPS* pps,
    base::span<const uint8_t> pps_nalu_data) {
  DVLOG(3) << __func__
           << ": pps_pic_parameter_set_id=" << pps->pps_pic_parameter_set_id
           << " pps_seq_parameter_set_id=" << pps->pps_seq_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  seen_pps_data_[pps->pps_pic_parameter_set_id] =
      std::vector<uint8_t>(pps_nalu_data.begin(), pps_nalu_data.end());
}

VideoToolboxH265Accelerator::Status
VideoToolboxH265Accelerator::SubmitFrameMetadata(
    const H265SPS* sps,
    const H265PPS* pps,
    const H265SliceHeader* slice_hdr,
    const H265Picture::Vector& ref_pic_list,
    const H265Picture::Vector& ref_pic_set_lt_curr,
    const H265Picture::Vector& ref_pic_set_st_curr_after,
    const H265Picture::Vector& ref_pic_set_st_curr_before,
    scoped_refptr<H265Picture> pic) {
  DVLOG(3) << __func__ << ":"
           << " sps_video_parameter_set_id=" << sps->sps_video_parameter_set_id
           << " sps_seq_parameter_set_id=" << sps->sps_seq_parameter_set_id
           << " pps_pic_parameter_set_id=" << pps->pps_pic_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  slice_nalu_data_.clear();

  // H265Decoder ignores VPS, so it doesn't check whether a valid one was
  // provided.
  if (!seen_vps_data_.contains(sps->sps_video_parameter_set_id)) {
    MEDIA_LOG(ERROR, media_log_.get()) << "Missing VPS";
    return Status::kFail;
  }

  // Detect format changes.
  DCHECK(seen_sps_data_.contains(sps->sps_seq_parameter_set_id));
  DCHECK(seen_pps_data_.contains(pps->pps_pic_parameter_set_id));
  std::vector<uint8_t>& vps_data =
      seen_vps_data_[sps->sps_video_parameter_set_id];
  std::vector<uint8_t>& sps_data =
      seen_sps_data_[sps->sps_seq_parameter_set_id];
  std::vector<uint8_t>& pps_data =
      seen_pps_data_[pps->pps_pic_parameter_set_id];
  if (vps_data != active_vps_data_ || sps_data != active_sps_data_ ||
      pps_data != active_pps_data_) {
    active_format_.reset();

    const uint8_t* nalu_data[3] = {vps_data.data(), sps_data.data(),
                                   pps_data.data()};
    size_t nalu_size[3] = {vps_data.size(), sps_data.size(), pps_data.size()};
    OSStatus status = CMVideoFormatDescriptionCreateFromHEVCParameterSets(
        kCFAllocatorDefault,
        3,                  // parameter_set_count
        nalu_data,          // parameter_set_pointers
        nalu_size,          // parameter_set_sizes
        kNALUHeaderLength,  // nal_unit_header_length
        nullptr,            // extensions
        active_format_.InitializeInto());
    if (status != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
          << "CMVideoFormatDescriptionCreateFromHEVCParameterSets()";
      return Status::kFail;
    }

    active_vps_data_ = vps_data;
    active_sps_data_ = sps_data;
    active_pps_data_ = pps_data;

    session_metadata_ = VideoToolboxSessionMetadata{
        /*allow_software_decoding=*/true,
        /*is_hbd=*/sps->bit_depth_y > 8,
    };
  }

  return Status::kOk;
}

VideoToolboxH265Accelerator::Status VideoToolboxH265Accelerator::SubmitSlice(
    const H265SPS* sps,
    const H265PPS* pps,
    const H265SliceHeader* slice_hdr,
    const H265Picture::Vector& ref_pic_list0,
    const H265Picture::Vector& ref_pic_list1,
    const H265Picture::Vector& ref_pic_set_lt_curr,
    const H265Picture::Vector& ref_pic_set_st_curr_after,
    const H265Picture::Vector& ref_pic_set_st_curr_before,
    scoped_refptr<H265Picture> pic,
    const uint8_t* data,
    size_t size,
    const std::vector<SubsampleEntry>& subsamples) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1331597): Implement kMinOutputsBeforeRASL workaround.
  slice_nalu_data_.push_back(base::make_span(data, size));
  return Status::kOk;
}

VideoToolboxH265Accelerator::Status VideoToolboxH265Accelerator::SubmitDecode(
    scoped_refptr<H265Picture> pic) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Determine the final size of the converted bitstream.
  size_t data_size = 0;
  for (const auto& nalu_data : slice_nalu_data_) {
    data_size += kNALUHeaderLength + nalu_data.size();
  }

  // Allocate a buffer.
  base::apple::ScopedCFTypeRef<CMBlockBufferRef> data;
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      kCFAllocatorDefault,
      nullptr,              // memory_block
      data_size,            // block_length
      kCFAllocatorDefault,  // block_allocator
      nullptr,              // custom_block_source
      0,                    // offset_to_data
      data_size,            // data_length
      0,                    // flags
      data.InitializeInto());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMBlockBufferCreateWithMemoryBlock()";
    return Status::kFail;
  }

  status = CMBlockBufferAssureBlockMemory(data);
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMBlockBufferAssureBlockMemory()";
    return Status::kFail;
  }

  // Copy each NALU into the buffer, prefixed with a length header.
  size_t offset = 0;
  for (const auto& nalu_data : slice_nalu_data_) {
    // Write length header.
    uint32_t header =
        base::HostToNet32(static_cast<uint32_t>(nalu_data.size()));
    status =
        CMBlockBufferReplaceDataBytes(&header, data, offset, kNALUHeaderLength);
    if (status != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
          << "CMBlockBufferReplaceDataBytes()";
      return Status::kFail;
    }
    offset += kNALUHeaderLength;

    // Write NALU data.
    status = CMBlockBufferReplaceDataBytes(nalu_data.data(), data, offset,
                                           nalu_data.size());
    if (status != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
          << "CMBlockBufferReplaceDataBytes()";
      return Status::kFail;
    }
    offset += nalu_data.size();
  }

  // Wrap in a sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample;
  status = CMSampleBufferCreate(kCFAllocatorDefault,
                                data,            // data_buffer
                                true,            // data_ready
                                nullptr,         // make_data_ready_callback
                                nullptr,         // make_data_ready_refcon
                                active_format_,  // format_description
                                1,               // num_samples
                                0,               // num_sample_timing_entries
                                nullptr,         // sample_timing_array
                                1,               // num_sample_size_entries
                                &data_size,      // sample_size_array
                                sample.InitializeInto());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMSampleBufferCreate()";
    return Status::kFail;
  }

  decode_cb_.Run(std::move(sample), session_metadata_, std::move(pic));
  return Status::kOk;
}

bool VideoToolboxH265Accelerator::OutputPicture(
    scoped_refptr<H265Picture> pic) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We don't care about outputs, just pass them along.
  output_cb_.Run(std::move(pic));
  return true;
}

void VideoToolboxH265Accelerator::Reset() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  seen_vps_data_.clear();
  seen_sps_data_.clear();
  seen_pps_data_.clear();
  active_vps_data_.clear();
  active_sps_data_.clear();
  active_pps_data_.clear();
  active_format_.reset();
  slice_nalu_data_.clear();
}

bool VideoToolboxH265Accelerator::IsChromaSamplingSupported(
    VideoChromaSampling format) {
  return true;
}

}  // namespace media
