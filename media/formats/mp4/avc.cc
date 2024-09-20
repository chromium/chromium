// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp4/avc.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "media/base/decrypt_config.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/box_reader.h"
#include "media/parsers/h264_parser.h"

namespace media {
namespace mp4 {

static constexpr uint8_t kAnnexBStartCode[] = {0, 0, 0, 1};
static constexpr int kAnnexBStartCodeSize = 4;

static bool ConvertAVCToAnnexBInPlaceForLengthSize4(std::vector<uint8_t>* buf) {
  const size_t kLengthSize = 4;
  size_t pos = 0;
  while (buf->size() > kLengthSize && buf->size() - kLengthSize > pos) {
    uint32_t nal_length = (*buf)[pos];
    nal_length = (nal_length << 8) + (*buf)[pos+1];
    nal_length = (nal_length << 8) + (*buf)[pos+2];
    nal_length = (nal_length << 8) + (*buf)[pos+3];

    if (nal_length == 0) {
      DVLOG(3) << "nal_length is 0";
      return false;
    }

    base::ranges::copy(kAnnexBStartCode, buf->begin() + pos);
    pos += kLengthSize + nal_length;
  }
  return pos == buf->size();
}

// static
int AVC::FindSubsampleIndex(const std::vector<uint8_t>& buffer,
                            const std::vector<SubsampleEntry>* subsamples,
                            const uint8_t* ptr) {
  DCHECK(ptr >= &buffer[0]);
  DCHECK(ptr <= &buffer[buffer.size()-1]);
  if (!subsamples || subsamples->empty())
    return 0;

  const uint8_t* p = &buffer[0];
  for (size_t i = 0; i < subsamples->size(); ++i) {
    p += (*subsamples)[i].clear_bytes + (*subsamples)[i].cypher_bytes;
    if (p > ptr)
      return i;
  }
  NOTREACHED();
}

// static
bool AVC::ConvertFrameToAnnexB(size_t length_size,
                               std::vector<uint8_t>* buffer,
                               std::vector<SubsampleEntry>* subsamples) {
  RCHECK(length_size == 1 || length_size == 2 || length_size == 4);
  DVLOG(5) << __func__ << " length_size=" << length_size
           << " buffer->size()=" << buffer->size()
           << " subsamples=" << (subsamples ? subsamples->size() : 0);

  if (length_size == 4)
    return ConvertAVCToAnnexBInPlaceForLengthSize4(buffer);

  std::vector<uint8_t> temp;
  temp.swap(*buffer);
  buffer->reserve(temp.size() + 32);

  size_t pos = 0;
  while (temp.size() > length_size && temp.size() - length_size > pos) {
    size_t nal_length = temp[pos];
    if (length_size == 2) nal_length = (nal_length << 8) + temp[pos+1];
    pos += length_size;

    if (nal_length == 0) {
      DVLOG(3) << "nal_length is 0";
      return false;
    }

    RCHECK(temp.size() >= nal_length && temp.size() - nal_length >= pos);
    buffer->insert(buffer->end(), kAnnexBStartCode,
                   kAnnexBStartCode + kAnnexBStartCodeSize);
    if (subsamples && !subsamples->empty()) {
      uint8_t* buffer_pos = &(*(buffer->end() - kAnnexBStartCodeSize));
      int subsample_index = FindSubsampleIndex(*buffer, subsamples, buffer_pos);
      // We've replaced NALU size value with an AnnexB start code.
      int size_adjustment = kAnnexBStartCodeSize - length_size;
      (*subsamples)[subsample_index].clear_bytes += size_adjustment;
    }
    buffer->insert(buffer->end(), temp.begin() + pos,
                   temp.begin() + pos + nal_length);
    pos += nal_length;
  }
  return pos == temp.size();
}

// static
bool AVC::InsertParamSetsAnnexB(const AVCDecoderConfigurationRecord& avc_config,
                                std::vector<uint8_t>* buffer,
                                std::vector<SubsampleEntry>* subsamples) {
  std::unique_ptr<H264Parser> parser(new H264Parser());
  const uint8_t* start = &(*buffer)[0];
  parser->SetEncryptedStream(start, buffer->size(), *subsamples);

  H264NALU nalu;
  if (parser->AdvanceToNextNALU(&nalu) != H264Parser::kOk)
    return false;

  std::vector<uint8_t>::iterator config_insert_point = buffer->begin();

  if (nalu.nal_unit_type == H264NALU::kAUD) {
    // Move insert point to just after the AUD.
    config_insert_point +=
        (nalu.data + base::checked_cast<size_t>(nalu.size)) - start;
  }

  // Clear |parser| and |start| since they aren't needed anymore and
  // will hold stale pointers once the insert happens.
  parser.reset();
  start = NULL;

  std::vector<uint8_t> param_sets;
  RCHECK(AVC::ConvertConfigToAnnexB(avc_config, &param_sets));

  if (subsamples && !subsamples->empty()) {
    if (config_insert_point != buffer->end()) {
      int subsample_index =
          FindSubsampleIndex(*buffer, subsamples, &(*config_insert_point));
      // Update the size of the subsample where SPS/PPS is to be inserted.
      (*subsamples)[subsample_index].clear_bytes += param_sets.size();
    } else {
      int subsample_index = (*subsamples).size() - 1;
      if ((*subsamples)[subsample_index].cypher_bytes == 0) {
        // Extend the last clear range to include the inserted data.
        (*subsamples)[subsample_index].clear_bytes += param_sets.size();
      } else {
        // Append a new subsample to cover the inserted data.
        (*subsamples).emplace_back(param_sets.size(), 0);
      }
    }
  }

  buffer->insert(config_insert_point,
                 param_sets.begin(), param_sets.end());
  return true;
}

// static
bool AVC::ConvertConfigToAnnexB(const AVCDecoderConfigurationRecord& avc_config,
                                std::vector<uint8_t>* buffer) {
  DCHECK(buffer->empty());
  buffer->clear();
  int total_size = 0;
  for (size_t i = 0; i < avc_config.sps_list.size(); i++)
    total_size += avc_config.sps_list[i].size() + kAnnexBStartCodeSize;
  for (size_t i = 0; i < avc_config.pps_list.size(); i++)
    total_size += avc_config.pps_list[i].size() + kAnnexBStartCodeSize;
  buffer->reserve(total_size);

  for (size_t i = 0; i < avc_config.sps_list.size(); i++) {
    buffer->insert(buffer->end(), kAnnexBStartCode,
                kAnnexBStartCode + kAnnexBStartCodeSize);
    buffer->insert(buffer->end(), avc_config.sps_list[i].begin(),
                avc_config.sps_list[i].end());
  }

  for (size_t i = 0; i < avc_config.pps_list.size(); i++) {
    buffer->insert(buffer->end(), kAnnexBStartCode,
                   kAnnexBStartCode + kAnnexBStartCodeSize);
    buffer->insert(buffer->end(), avc_config.pps_list[i].begin(),
                   avc_config.pps_list[i].end());
  }
  return true;
}

// static
BitstreamConverter::AnalysisResult AVC::AnalyzeAnnexB(
    const uint8_t* buffer,
    size_t size,
    const std::vector<SubsampleEntry>& subsamples) {
  DVLOG(3) << __func__;
  DCHECK(buffer);

  BitstreamConverter::AnalysisResult result;
  result.is_conformant = false;  // Will change if needed before return.

  if (size == 0) {
    result.is_conformant = true;
    return result;
  }

  H264Parser parser;
  parser.SetEncryptedStream(buffer, size, subsamples);

  typedef enum {
    kAUDAllowed,
    kBeforeFirstVCL,  // VCL == nal_unit_types 1-5
    kAfterFirstVCL,
    kEOStreamAllowed,
    kNoMoreDataAllowed,
  } NALUOrderState;

  H264NALU nalu;
  NALUOrderState order_state = kAUDAllowed;
  int last_nalu_type = H264NALU::kUnspecified;
  bool done = false;
  while (!done) {
    switch (parser.AdvanceToNextNALU(&nalu)) {
      case H264Parser::kOk:
        DVLOG(3) << "nal_unit_type " << nalu.nal_unit_type;

        switch (nalu.nal_unit_type) {
          case H264NALU::kAUD:
            if (order_state > kAUDAllowed) {
              DVLOG(1) << "Unexpected AUD in order_state " << order_state;
              return result;
            }
            order_state = kBeforeFirstVCL;
            break;

          case H264NALU::kSEIMessage:
          case H264NALU::kPrefix:
          case H264NALU::kSubsetSPS:
          case H264NALU::kDPS:
          case H264NALU::kReserved17:
          case H264NALU::kReserved18:
          case H264NALU::kPPS:
          case H264NALU::kSPS:
            if (order_state > kBeforeFirstVCL) {
              DVLOG(1) << "Unexpected NALU type " << nalu.nal_unit_type
                       << " in order_state " << order_state;
              return result;
            }
            order_state = kBeforeFirstVCL;
            break;

          case H264NALU::kSPSExt:
            if (last_nalu_type != H264NALU::kSPS) {
              DVLOG(1) << "SPS extension does not follow an SPS.";
              return result;
            }
            break;

          case H264NALU::kNonIDRSlice:
          case H264NALU::kSliceDataA:
          case H264NALU::kSliceDataB:
          case H264NALU::kSliceDataC:
          case H264NALU::kIDRSlice:
            if (order_state > kAfterFirstVCL) {
              DVLOG(1) << "Unexpected VCL in order_state " << order_state;
              return result;
            }

            if (!result.is_keyframe.has_value())
              result.is_keyframe = nalu.nal_unit_type == H264NALU::kIDRSlice;

            order_state = kAfterFirstVCL;
            break;

          case H264NALU::kCodedSliceAux:
            if (order_state != kAfterFirstVCL) {
              DVLOG(1) << "Unexpected extension in order_state " << order_state;
              return result;
            }
            break;

          case H264NALU::kEOSeq:
            if (order_state != kAfterFirstVCL) {
              DVLOG(1) << "Unexpected EOSeq in order_state " << order_state;
              return result;
            }
            order_state = kEOStreamAllowed;
            break;

          case H264NALU::kEOStream:
            if (order_state < kAfterFirstVCL) {
              DVLOG(1) << "Unexpected EOStream in order_state " << order_state;
              return result;
            }
            order_state = kNoMoreDataAllowed;
            break;

          case H264NALU::kFiller:
          case H264NALU::kUnspecified:
            // These syntax elements are to simply be ignored according to H264
            // Annex B 7.4.2.7
            break;

          default:
            DCHECK_GE(nalu.nal_unit_type, 20);
            if (nalu.nal_unit_type >= 20 && nalu.nal_unit_type <= 31 &&
                order_state != kAfterFirstVCL) {
              DVLOG(1) << "Unexpected NALU type " << nalu.nal_unit_type
                       << " in order_state " << order_state;
              return result;
            }
        }
        last_nalu_type = nalu.nal_unit_type;
        break;

      case H264Parser::kInvalidStream:
        return result;

      case H264Parser::kUnsupportedStream:
        NOTREACHED_IN_MIGRATION()
            << "AdvanceToNextNALU() returned kUnsupportedStream!";
        return result;

      case H264Parser::kEOStream:
        done = true;
    }
  }

  if (order_state < kAfterFirstVCL)
    return result;

  result.is_conformant = true;
  DCHECK(result.is_keyframe.has_value());
  return result;
}

AVCBitstreamConverter::AVCBitstreamConverter(
    std::unique_ptr<AVCDecoderConfigurationRecord> avc_config)
    : avc_config_(std::move(avc_config)) {
  DCHECK(avc_config_);
}

AVCBitstreamConverter::~AVCBitstreamConverter() = default;

bool AVCBitstreamConverter::ConvertAndAnalyzeFrame(
    std::vector<uint8_t>* frame_buf,
    bool is_keyframe,
    std::vector<SubsampleEntry>* subsamples,
    AnalysisResult* analysis_result) const {
  // Convert the AVC NALU length fields to Annex B headers, as expected by
  // decoding libraries. Since this may enlarge the size of the buffer, we also
  // update the clear byte count for each subsample if encryption is used to
  // account for the difference in size between the length prefix and Annex B
  // start code.
  RCHECK(AVC::ConvertFrameToAnnexB(avc_config_->length_size, frame_buf,
                                   subsamples));

  // |is_keyframe| may be incorrect. Analyze the frame to see if it is a
  // keyframe. |is_keyframe| will be used if the analysis is inconclusive.
  // Also, provide the analysis result to the caller via out parameter
  // |analysis_result|.
  *analysis_result = Analyze(frame_buf, subsamples);

  if (analysis_result->is_keyframe.value_or(is_keyframe)) {
    // If this is a keyframe, we (re-)inject SPS and PPS headers at the start of
    // a frame. If subsample info is present, we also update the clear byte
    // count for that first subsample.
    RCHECK(AVC::InsertParamSetsAnnexB(*avc_config_, frame_buf, subsamples));
  }

  return true;
}

BitstreamConverter::AnalysisResult AVCBitstreamConverter::Analyze(
    std::vector<uint8_t>* frame_buf,
    std::vector<SubsampleEntry>* subsamples) const {
  return AVC::AnalyzeAnnexB(frame_buf->data(), frame_buf->size(), *subsamples);
}

}  // namespace mp4
}  // namespace media
