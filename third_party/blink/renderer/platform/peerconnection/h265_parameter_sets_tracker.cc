// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/peerconnection/h265_parameter_sets_tracker.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "third_party/webrtc/common_video/h265/h265_common.h"
#include "third_party/webrtc/common_video/h265/h265_pps_parser.h"
#include "third_party/webrtc/common_video/h265/h265_sps_parser.h"
#include "third_party/webrtc/common_video/h265/h265_vps_parser.h"

namespace blink {

namespace {
constexpr size_t kMaxParameterSetSizeBytes = 1024;
}

H265ParameterSetsTracker::H265ParameterSetsTracker() = default;
H265ParameterSetsTracker::~H265ParameterSetsTracker() = default;

H265ParameterSetsTracker::PpsData::PpsData() = default;
H265ParameterSetsTracker::PpsData::PpsData(PpsData&& rhs) = default;
H265ParameterSetsTracker::PpsData& H265ParameterSetsTracker::PpsData::operator=(
    PpsData&& rhs) = default;
H265ParameterSetsTracker::PpsData::~PpsData() = default;

H265ParameterSetsTracker::SpsData::SpsData() = default;
H265ParameterSetsTracker::SpsData::SpsData(SpsData&& rhs) = default;
H265ParameterSetsTracker::SpsData& H265ParameterSetsTracker::SpsData::operator=(
    SpsData&& rhs) = default;
H265ParameterSetsTracker::SpsData::~SpsData() = default;

H265ParameterSetsTracker::VpsData::VpsData() = default;
H265ParameterSetsTracker::VpsData::VpsData(VpsData&& rhs) = default;
H265ParameterSetsTracker::VpsData& H265ParameterSetsTracker::VpsData::operator=(
    VpsData&& rhs) = default;
H265ParameterSetsTracker::VpsData::~VpsData() = default;

H265ParameterSetsTracker::FixedBitstream
H265ParameterSetsTracker::MaybeFixBitstream(
    rtc::ArrayView<const uint8_t> bitstream) {
  if (!bitstream.size()) {
    return {PacketAction::kRequestKeyframe};
  }

  bool has_irap_nalu = false;
  bool prepend_vps = true, prepend_sps = true, prepend_pps = true;

  // Required size of fixed bitstream.
  size_t required_size = 0;
  H265ParameterSetsTracker::FixedBitstream fixed;
  fixed.action = PacketAction::kPassThrough;

  auto vps_data = vps_data_.end();
  auto sps_data = sps_data_.end();
  auto pps_data = pps_data_.end();
  std::optional<uint32_t> pps_id;
  uint32_t sps_id = 0, vps_id = 0;
  uint32_t slice_sps_id = 0, slice_pps_id = 0;

  parser_.ParseBitstream(
      rtc::ArrayView<const uint8_t>(bitstream.data(), bitstream.size()));

  std::vector<webrtc::H265::NaluIndex> nalu_indices =
      webrtc::H265::FindNaluIndices(bitstream.data(), bitstream.size());
  for (const auto& nalu_index : nalu_indices) {
    if (nalu_index.payload_size < 2) {
      // H.265 NALU header is at least 2 bytes.
      return {PacketAction::kRequestKeyframe};
    }
    const uint8_t* payload_start =
        bitstream.data() + nalu_index.payload_start_offset;
    const uint8_t* nalu_start = bitstream.data() + nalu_index.start_offset;
    size_t nalu_size = nalu_index.payload_size +
                       nalu_index.payload_start_offset -
                       nalu_index.start_offset;
    uint8_t nalu_type = webrtc::H265::ParseNaluType(payload_start[0]);

    std::optional<webrtc::H265VpsParser::VpsState> vps;
    std::optional<webrtc::H265SpsParser::SpsState> sps;

    switch (nalu_type) {
      case webrtc::H265::NaluType::kVps:
        // H.265 parameter set parsers expect NALU header already stripped.
        vps = webrtc::H265VpsParser::ParseVps(payload_start + 2,
                                              nalu_index.payload_size - 2);
        // Always replace VPS with the same ID. Same for other parameter sets.
        if (vps) {
          std::unique_ptr<VpsData> current_vps_data =
              std::make_unique<VpsData>();
          // Copy with start code included. Same for other parameter sets.
          if (!current_vps_data.get() || !nalu_size ||
              nalu_size > kMaxParameterSetSizeBytes) {
            return {PacketAction::kRequestKeyframe};
          }
          current_vps_data->size = nalu_size;
          uint8_t* vps_payload = new uint8_t[current_vps_data->size];
          memcpy(vps_payload, nalu_start, current_vps_data->size);
          current_vps_data->payload.reset(vps_payload);
          vps_data_.Set(vps->id, std::move(current_vps_data));
        }
        prepend_vps = false;
        break;
      case webrtc::H265::NaluType::kSps:
        sps = webrtc::H265SpsParser::ParseSps(payload_start + 2,
                                              nalu_index.payload_size - 2);
        if (sps) {
          std::unique_ptr<SpsData> current_sps_data =
              std::make_unique<SpsData>();
          if (!current_sps_data.get() || !nalu_size ||
              nalu_size > kMaxParameterSetSizeBytes) {
            return {PacketAction::kRequestKeyframe};
          }
          current_sps_data->size = nalu_size;
          current_sps_data->vps_id = sps->vps_id;
          uint8_t* sps_payload = new uint8_t[current_sps_data->size];
          memcpy(sps_payload, nalu_start, current_sps_data->size);
          current_sps_data->payload.reset(sps_payload);
          sps_data_.Set(sps->sps_id, std::move(current_sps_data));
        }
        prepend_sps = false;
        break;
      case webrtc::H265::NaluType::kPps:
        if (webrtc::H265PpsParser::ParsePpsIds(payload_start + 2,
                                               nalu_index.payload_size - 2,
                                               &slice_pps_id, &slice_sps_id)) {
          auto current_sps_data = sps_data_.find(slice_sps_id);
          if (current_sps_data == sps_data_.end()) {
            DLOG(WARNING) << "No SPS associated with current parsed PPS found.";
            fixed.action = PacketAction::kRequestKeyframe;
          } else {
            std::unique_ptr<PpsData> current_pps_data =
                std::make_unique<PpsData>();
            if (!current_pps_data.get() || !nalu_size ||
                nalu_size > kMaxParameterSetSizeBytes) {
              return {PacketAction::kRequestKeyframe};
            }
            current_pps_data->size = nalu_size;
            current_pps_data->sps_id = slice_sps_id;
            uint8_t* pps_payload = new uint8_t[current_pps_data->size];
            memcpy(pps_payload, nalu_start, current_pps_data->size);
            current_pps_data->payload.reset(pps_payload);
            pps_data_.Set(slice_pps_id, std::move(current_pps_data));
          }
          prepend_pps = false;
        }
        break;
      case webrtc::H265::NaluType::kBlaWLp:
      case webrtc::H265::NaluType::kBlaWRadl:
      case webrtc::H265::NaluType::kBlaNLp:
      case webrtc::H265::NaluType::kIdrWRadl:
      case webrtc::H265::NaluType::kIdrNLp:
      case webrtc::H265::NaluType::kCra:
        has_irap_nalu = true;
        pps_id = parser_.GetLastSlicePpsId();
        if (!pps_id) {
          DLOG(WARNING) << "Failed to parse PPS id from current slice.";
          fixed.action = PacketAction::kRequestKeyframe;
          break;
        }
        pps_data = pps_data_.find(pps_id.value());
        if (pps_data == pps_data_.end()) {
          DLOG(WARNING) << "PPS associated with current slice is not found.";
          fixed.action = PacketAction::kRequestKeyframe;
          break;
        }

        sps_id = (pps_data->value)->sps_id;
        sps_data = sps_data_.find(sps_id);
        if (sps_data == sps_data_.end()) {
          DLOG(WARNING) << "SPS associated with current slice is not found.";
          fixed.action = PacketAction::kRequestKeyframe;
          break;
        }

        vps_id = (sps_data->value)->vps_id;
        vps_data = vps_data_.find(vps_id);
        if (vps_data == vps_data_.end()) {
          DLOG(WARNING) << "VPS associated with current slice is not found.";
          fixed.action = PacketAction::kRequestKeyframe;
          break;
        }

        if (!prepend_vps && !prepend_sps && !prepend_pps) {
          fixed.action = PacketAction::kPassThrough;
        } else {
          required_size += vps_data->value->size + sps_data->value->size +
                           pps_data->value->size;

          required_size += bitstream.size();
          size_t offset = 0;

          fixed.bitstream = webrtc::EncodedImageBuffer::Create(required_size);
          memcpy(fixed.bitstream->data(), vps_data->value->payload.get(),
                 vps_data->value->size);
          offset += vps_data->value->size;
          memcpy(fixed.bitstream->data() + offset,
                 sps_data->value->payload.get(), sps_data->value->size);
          offset += sps_data->value->size;
          memcpy(fixed.bitstream->data() + offset,
                 pps_data->value->payload.get(), pps_data->value->size);
          offset += pps_data->value->size;
          memcpy(fixed.bitstream->data() + offset, bitstream.data(),
                 bitstream.size());

          fixed.action = PacketAction::kInsert;
        }
        break;
      default:
        break;
    }

    if (fixed.action == PacketAction::kRequestKeyframe) {
      return {PacketAction::kRequestKeyframe};
    } else if (fixed.action == PacketAction::kInsert) {
      return fixed;
    }

    if (has_irap_nalu) {
      break;
    }
  }

  fixed.action = PacketAction::kPassThrough;

  return fixed;
}

}  // namespace blink
