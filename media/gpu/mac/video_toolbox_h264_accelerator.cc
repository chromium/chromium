// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_h264_accelerator.h"

#include <utility>

#include "base/sys_byteorder.h"
#include "media/base/media_log.h"

namespace media {

namespace {
constexpr size_t kNALUHeaderLength = 4;
}  // namespace

VideoToolboxH264Accelerator::VideoToolboxH264Accelerator(
    std::unique_ptr<MediaLog> media_log,
    DecodeCB decode_cb,
    OutputCB output_cb)
    : media_log_(std::move(media_log)),
      decode_cb_(std::move(decode_cb)),
      output_cb_(std::move(output_cb)) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

VideoToolboxH264Accelerator::~VideoToolboxH264Accelerator() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<H264Picture> VideoToolboxH264Accelerator::CreateH264Picture() {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::MakeRefCounted<H264Picture>();
}

void VideoToolboxH264Accelerator::ProcessSPS(
    const H264SPS* sps,
    base::span<const uint8_t> sps_nalu_data) {
  DVLOG(3) << __func__
           << ": seq_parameter_set_id=" << sps->seq_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  seen_sps_data_[sps->seq_parameter_set_id] =
      std::vector<uint8_t>(sps_nalu_data.begin(), sps_nalu_data.end());
}

void VideoToolboxH264Accelerator::ProcessPPS(
    const H264PPS* pps,
    base::span<const uint8_t> pps_nalu_data) {
  DVLOG(3) << __func__ << ": pic_parameter_set_id=" << pps->pic_parameter_set_id
           << " seq_parameter_set_id=" << pps->seq_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  seen_pps_data_[pps->pic_parameter_set_id] =
      std::vector<uint8_t>(pps_nalu_data.begin(), pps_nalu_data.end());
}

VideoToolboxH264Accelerator::Status
VideoToolboxH264Accelerator::SubmitFrameMetadata(
    const H264SPS* sps,
    const H264PPS* pps,
    const H264DPB& dpb,
    const H264Picture::Vector& ref_pic_listp0,
    const H264Picture::Vector& ref_pic_listb0,
    const H264Picture::Vector& ref_pic_listb1,
    scoped_refptr<H264Picture> pic) {
  DVLOG(3) << __func__ << ": seq_parameter_set_id=" << sps->seq_parameter_set_id
           << " pic_parameter_set_id=" << pps->pic_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  slice_nalu_data_.clear();

  // Detect format changes.
  DCHECK(seen_sps_data_.contains(sps->seq_parameter_set_id));
  DCHECK(seen_pps_data_.contains(pps->pic_parameter_set_id));
  std::vector<uint8_t>& sps_data = seen_sps_data_[sps->seq_parameter_set_id];
  std::vector<uint8_t>& pps_data = seen_pps_data_[pps->pic_parameter_set_id];
  if (sps_data != active_sps_data_ || pps_data != active_pps_data_) {
    // If we're not at a keyframe and only the PPS has changed, put the new PPS
    // in-band and don't create a new format.
    // TODO(crbug.com/1331597): Record that this PPS has been provided and avoid
    // sending it again.
    if (!pic->idr && sps_data == active_sps_data_) {
      slice_nalu_data_.push_back(
          base::make_span(pps_data.data(), pps_data.size()));
      return Status::kOk;
    }

    active_format_.reset();

    const uint8_t* nalu_data[2] = {sps_data.data(), pps_data.data()};
    size_t nalu_size[2] = {sps_data.size(), pps_data.size()};
    OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
        kCFAllocatorDefault,
        2,                  // parameter_set_count
        nalu_data,          // parameter_set_pointers
        nalu_size,          // parameter_set_sizes
        kNALUHeaderLength,  // nal_unit_header_length
        active_format_.InitializeInto());
    if (status != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
          << "CMVideoFormatDescriptionCreateFromH264ParameterSets()";
      return Status::kFail;
    }

    active_sps_data_ = sps_data;
    active_pps_data_ = pps_data;
  }

  return Status::kOk;
}

VideoToolboxH264Accelerator::Status VideoToolboxH264Accelerator::SubmitSlice(
    const H264PPS* pps,
    const H264SliceHeader* slice_hdr,
    const H264Picture::Vector& ref_pic_list0,
    const H264Picture::Vector& ref_pic_list1,
    scoped_refptr<H264Picture> pic,
    const uint8_t* data,
    size_t size,
    const std::vector<SubsampleEntry>& subsamples) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  slice_nalu_data_.push_back(base::make_span(data, size));
  return Status::kOk;
}

VideoToolboxH264Accelerator::Status VideoToolboxH264Accelerator::SubmitDecode(
    scoped_refptr<H264Picture> pic) {
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

  status = CMBlockBufferAssureBlockMemory(data.get());
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
    status = CMBlockBufferReplaceDataBytes(&header, data.get(), offset,
                                           kNALUHeaderLength);
    if (status != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
          << "CMBlockBufferReplaceDataBytes()";
      return Status::kFail;
    }
    offset += kNALUHeaderLength;

    // Write NALU data.
    status = CMBlockBufferReplaceDataBytes(nalu_data.data(), data.get(), offset,
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
                                data.get(),  // data_buffer
                                true,        // data_ready
                                nullptr,     // make_data_ready_callback
                                nullptr,     // make_data_ready_refcon
                                active_format_.get(),  // format_description
                                1,                     // num_samples
                                0,           // num_sample_timing_entries
                                nullptr,     // sample_timing_array
                                1,           // num_sample_size_entries
                                &data_size,  // sample_size_array
                                sample.InitializeInto());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMSampleBufferCreate()";
    return Status::kFail;
  }

  VideoToolboxSessionMetadata session_metadata = {
      /*allow_software_decoding=*/false, /*is_hbd=*/false};
  decode_cb_.Run(std::move(sample), session_metadata, std::move(pic));
  return Status::kOk;
}

bool VideoToolboxH264Accelerator::OutputPicture(
    scoped_refptr<H264Picture> pic) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We don't care about outputs, just pass them along.
  output_cb_.Run(std::move(pic));
  return true;
}

void VideoToolboxH264Accelerator::Reset() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  seen_sps_data_.clear();
  seen_pps_data_.clear();
  active_sps_data_.clear();
  active_pps_data_.clear();
  active_format_.reset();
  slice_nalu_data_.clear();
}

}  // namespace media
