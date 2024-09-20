// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp2t/es_parser_h264.h"

#include <limits>
#include <optional>

#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/decrypt_config.h"
#include "media/base/encryption_pattern.h"
#include "media/base/media_util.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/formats/common/offset_byte_queue.h"
#include "media/formats/mp2t/mp2t_common.h"
#include "media/parsers/h264_parser.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace mp2t {

namespace {

const int kSampleAESMaxUnprotectedNALULength = 48;
const int kSampleAESClearLeaderSize = 32;
const int kSampleAESEncryptBlocks = 1;
const int kSampleAESSkipBlocks = 9;
const int kSampleAESPatternUnit =
    (kSampleAESEncryptBlocks + kSampleAESSkipBlocks) * 16;

// Attempts to find the first or only EP3B (emulation prevention 3 byte) in
// the part of the |buffer| between |start_pos| and |end_pos|. Returns the
// position of the EP3B, or 0 if there are none.
// Note: the EP3B always follows two zero bytes, so the value 0 can never be a
// valid position.
int FindEP3B(const uint8_t* buffer, int start_pos, int end_pos) {
  const uint8_t* data = buffer + start_pos;
  int data_size = end_pos - start_pos;
  DCHECK_GE(data_size, 0);
  int bytes_left = data_size;

  while (bytes_left >= 4) {
    if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x03 &&
        data[3] <= 0x03) {
      return (data - buffer) + 2;
    }
    ++data;
    --bytes_left;
  }
  return 0;
}

// Remove the byte at |pos| in the |buffer| and close up the gap, moving all the
// bytes from [pos + 1, end_pos) to [pos, end_pos - 1).
void RemoveByte(uint8_t* buffer, int pos, int end_pos) {
  memmove(&buffer[pos], &buffer[pos + 1], end_pos - pos - 1);
}

// Given an Access Unit pointed to by |au| of size |au_size|, removes emulation
// prevention 3 bytes (EP3B) from within the |protected_blocks|. Also computes
// the |subsamples| vector describing the resulting AU.
// Returns the allocated buffer holding the adjusted copy, or NULL if no size
// adjustment was necessary.
std::unique_ptr<uint8_t[]> AdjustAUForSampleAES(
    const uint8_t* au,
    int* au_size,
    const Ranges<int>& protected_blocks,
    std::vector<SubsampleEntry>* subsamples) {
  DCHECK(subsamples);
  DCHECK(au_size);
  std::unique_ptr<uint8_t[]> result;
  int& au_end_pos = *au_size;

  // 1. Considering each protected block in turn, find any emulation prevention
  // 3 bytes (EP3B) within it, keeping track of their positions. While doing so,
  // produce a revised Ranges<int> reflecting the new protected block positions
  // that will apply after we have removed the EP3Bs.
  Ranges<int> adjusted_protected_blocks;
  std::vector<int> epbs;
  int adjustment = 0;
  for (size_t i = 0; i < protected_blocks.size(); i++) {
    int start_pos = protected_blocks.start(i);
    int end_pos = protected_blocks.end(i);
    int search_pos = start_pos;
    int epb_pos;
    int block_adjustment = 0;
    while ((epb_pos = FindEP3B(au, search_pos, end_pos))) {
      epbs.push_back(epb_pos);
      block_adjustment++;
      search_pos = epb_pos + 2;
    }
    // adjust the start_pos and end_pos to accommodate the EPBs that will be
    // removed.
    start_pos -= adjustment;
    adjustment += block_adjustment;
    end_pos -= adjustment;
    if (end_pos - start_pos > kSampleAESMaxUnprotectedNALULength)
      adjusted_protected_blocks.Add(start_pos, end_pos);
    else
      VLOG(1) << "Ignoring short protected block of length: "
              << (end_pos - start_pos);
  }

  // 2. If we actually found any EP3Bs, make a copy of the AU and then remove
  // the EP3Bs in the copy (we can't modify the original).
  if (adjustment) {
    result.reset(new uint8_t[au_end_pos]);
    uint8_t* temp = result.get();
    memcpy(temp, au, au_end_pos);
    for (const auto& epb : base::Reversed(epbs)) {
      RemoveByte(temp, epb, au_end_pos);
      au_end_pos--;
    }
    au = temp;
    VLOG(2) << "Copied AU and removed emulation prevention bytes: "
            << adjustment;
  }

  // We now have either the original AU, or a copy with the EP3Bs removed.
  // We also have an updated Ranges<int> indicating the protected blocks.
  // Also au_end_pos has been adjusted to indicate the new au_size.

  // 3. Use a new Ranges<int> to collect all the clear ranges. They will
  // automatically be coalesced to minimize the number of (disjoint) ranges.
  Ranges<int> clear_ranges;
  int previous_pos = 0;
  for (size_t i = 0; i < adjusted_protected_blocks.size(); i++) {
    int start_pos = adjusted_protected_blocks.start(i);
    int end_pos = adjusted_protected_blocks.end(i);
    // Add the clear range prior to this protected block.
    clear_ranges.Add(previous_pos, start_pos);
    int block_size = end_pos - start_pos;
    DCHECK_GT(block_size, kSampleAESMaxUnprotectedNALULength);
    // Add the clear leader.
    clear_ranges.Add(start_pos, start_pos + kSampleAESClearLeaderSize);
    block_size -= kSampleAESClearLeaderSize;
    // The bytes beyond an integral multiple of AES blocks (16 bytes) are to be
    // left clear. Also, if the last 16 bytes would be the only block in a
    // pattern unit (160 bytes), they are also left clear.
    int residual_bytes = block_size % kSampleAESPatternUnit;
    if (residual_bytes > 16)
      residual_bytes = residual_bytes % 16;
    clear_ranges.Add(end_pos - residual_bytes, end_pos);
    previous_pos = end_pos;
  }
  // Add the trailing bytes, if any, beyond the last protected block.
  clear_ranges.Add(previous_pos, au_end_pos);

  // 4. Convert the disjoint set of clear ranges into subsample entries. Each
  // subsample entry is a count of clear bytes followed by a count of protected
  // bytes.
  subsamples->clear();
  for (size_t i = 0; i < clear_ranges.size(); i++) {
    int start_pos = clear_ranges.start(i);
    int end_pos = clear_ranges.end(i);
    int clear_size = end_pos - start_pos;
    int encrypt_end_pos = au_end_pos;

    if (i + 1 < clear_ranges.size())
      encrypt_end_pos = clear_ranges.start(i + 1);
    SubsampleEntry subsample(clear_size, encrypt_end_pos - end_pos);
    subsamples->push_back(subsample);
  }
  return result;
}

}  // namespace

// An AUD NALU is at least 4 bytes:
// 3 bytes for the start code + 1 byte for the NALU type.
constexpr int kMinAUDSize = 4;

EsParserH264::EsParserH264(NewVideoConfigCB new_video_config_cb,
                           EmitBufferCB emit_buffer_cb)
    : es_adapter_(std::move(new_video_config_cb), std::move(emit_buffer_cb)),
      h264_parser_(new H264Parser()),
      current_access_unit_pos_(0),
      next_access_unit_pos_(0),
      init_encryption_scheme_(EncryptionScheme::kUnencrypted),
      get_decrypt_config_cb_() {}

EsParserH264::EsParserH264(NewVideoConfigCB new_video_config_cb,
                           EmitBufferCB emit_buffer_cb,
                           EncryptionScheme init_encryption_scheme,
                           const GetDecryptConfigCB& get_decrypt_config_cb)
    : es_adapter_(std::move(new_video_config_cb), std::move(emit_buffer_cb)),
      h264_parser_(new H264Parser()),
      current_access_unit_pos_(0),
      next_access_unit_pos_(0),
      init_encryption_scheme_(init_encryption_scheme),
      get_decrypt_config_cb_(get_decrypt_config_cb) {}

EsParserH264::~EsParserH264() = default;

void EsParserH264::Flush() {
  DVLOG(1) << __func__;
  if (!FindAUD(&current_access_unit_pos_))
    return;

  // Simulate an additional AUD to force emitting the last access unit
  // which is assumed to be complete at this point.
  uint8_t aud[] = {0x00, 0x00, 0x01, 0x09};

  // Fail if this AUD's push fails allocation, since otherwise the behavior of
  // the subsequent parse would vary based on whether or not the system is
  // near-OOM.
  // TODO(crbug.com/40204179): Consider plumbing parse failure for this push
  // failure case, instead of what used to OOM but now instead would fail this
  // CHECK.
  CHECK(es_queue_->Push(base::make_span(aud, sizeof(aud))));

  ParseFromEsQueue();
  es_adapter_.Flush();
}

void EsParserH264::ResetInternal() {
  DVLOG(1) << __func__;
  h264_parser_.reset(new H264Parser());
  current_access_unit_pos_ = 0;
  next_access_unit_pos_ = 0;
  last_video_decoder_config_ = VideoDecoderConfig();
  es_adapter_.Reset();
}

bool EsParserH264::FindAUD(int64_t* stream_pos) {
  while (true) {
    const uint8_t* es;
    int size;
    es_queue_->PeekAt(*stream_pos, &es, &size);

    // Find a start code and move the stream to the start code parser position.
    off_t start_code_offset;
    off_t start_code_size;
    bool start_code_found = H264Parser::FindStartCode(
        es, size, &start_code_offset, &start_code_size);
    *stream_pos += start_code_offset;

    // No H264 start code found or NALU type not available yet.
    if (!start_code_found || start_code_offset + start_code_size >= size)
      return false;

    // Exit the parser loop when an AUD is found.
    // Note: NALU header for an AUD:
    // - nal_ref_idc must be 0
    // - nal_unit_type must be H264NALU::kAUD
    if (es[start_code_offset + start_code_size] == H264NALU::kAUD)
      break;

    // The current NALU is not an AUD, skip the start code
    // and continue parsing the stream.
    *stream_pos += start_code_size;
  }

  return true;
}

bool EsParserH264::ParseFromEsQueue() {
  DCHECK_LE(es_queue_->head(), current_access_unit_pos_);
  DCHECK_LE(current_access_unit_pos_, next_access_unit_pos_);
  DCHECK_LE(next_access_unit_pos_, es_queue_->tail());

  // Find the next AUD located at or after |current_access_unit_pos_|. This is
  // needed since initially |current_access_unit_pos_| might not point to
  // an AUD.
  // Discard all the data before the updated |current_access_unit_pos_|
  // since it won't be used again.
  bool aud_found = FindAUD(&current_access_unit_pos_);
  es_queue_->Trim(current_access_unit_pos_);
  if (next_access_unit_pos_ < current_access_unit_pos_)
    next_access_unit_pos_ = current_access_unit_pos_;

  // Resume parsing later if no AUD was found.
  if (!aud_found)
    return true;

  // Find the next AUD to make sure we have a complete access unit.
  if (next_access_unit_pos_ < current_access_unit_pos_ + kMinAUDSize) {
    next_access_unit_pos_ = current_access_unit_pos_ + kMinAUDSize;
    DCHECK_LE(next_access_unit_pos_, es_queue_->tail());
  }
  if (!FindAUD(&next_access_unit_pos_))
    return true;

  // At this point, we know we have a full access unit.
  bool is_key_frame = false;
  int pps_id_for_access_unit = -1;

  const uint8_t* es;
  int size;
  es_queue_->PeekAt(current_access_unit_pos_, &es, &size);
  int access_unit_size = base::checked_cast<int>(
      next_access_unit_pos_ - current_access_unit_pos_);
  DCHECK_LE(access_unit_size, size);
  h264_parser_->SetStream(es, access_unit_size);

  while (true) {
    bool is_eos = false;
    H264NALU nalu;
    switch (h264_parser_->AdvanceToNextNALU(&nalu)) {
      case H264Parser::kOk:
        break;
      case H264Parser::kInvalidStream:
      case H264Parser::kUnsupportedStream:
        return false;
      case H264Parser::kEOStream:
        is_eos = true;
        break;
    }
    if (is_eos)
      break;

    switch (nalu.nal_unit_type) {
      case H264NALU::kAUD: {
        DVLOG(LOG_LEVEL_ES) << "NALU: AUD";
        break;
      }
      case H264NALU::kSPS: {
        DVLOG(LOG_LEVEL_ES) << "NALU: SPS";
        int sps_id;
        if (h264_parser_->ParseSPS(&sps_id) != H264Parser::kOk)
          return false;
        break;
      }
      case H264NALU::kPPS: {
        DVLOG(LOG_LEVEL_ES) << "NALU: PPS";
        int pps_id;
        if (h264_parser_->ParsePPS(&pps_id) != H264Parser::kOk) {
          // Allow PPS parsing to fail if SPS have not been parsed yet,
          // since it is possible to have a PPS before SPS in the stream.
          if (last_video_decoder_config_.IsValidConfig())
            return false;
        }
        break;
      }
      case H264NALU::kIDRSlice:
      case H264NALU::kNonIDRSlice: {
        is_key_frame = (nalu.nal_unit_type == H264NALU::kIDRSlice);
        DVLOG(LOG_LEVEL_ES) << "NALU: slice IDR=" << is_key_frame;
        H264SliceHeader shdr;
        if (h264_parser_->ParseSliceHeader(nalu, &shdr) != H264Parser::kOk) {
          // Only accept an invalid SPS/PPS at the beginning when the stream
          // does not necessarily start with an SPS/PPS/IDR.
          // TODO(damienv): Should be able to differentiate a missing SPS/PPS
          // from a slice header parsing error.
          if (last_video_decoder_config_.IsValidConfig())
            return false;
        } else {
          pps_id_for_access_unit = shdr.pic_parameter_set_id;
        }
        // With HLS SampleAES, protected blocks in H.264 consist of IDR and non-
        // IDR slices that are more than 48 bytes in length.
        if (get_decrypt_config_cb_ && get_decrypt_config_cb_.Run() &&
            nalu.size > kSampleAESMaxUnprotectedNALULength) {
          int64_t nal_begin = nalu.data - es;
          protected_blocks_.Add(nal_begin, nal_begin + nalu.size);
        }
        break;
      }
      default: {
        DVLOG(LOG_LEVEL_ES) << "NALU: " << nalu.nal_unit_type;
      }
    }
  }

  // Emit a frame and move the stream to the next AUD position.
  RCHECK(EmitFrame(current_access_unit_pos_, access_unit_size,
                   is_key_frame, pps_id_for_access_unit));
  current_access_unit_pos_ = next_access_unit_pos_;
  es_queue_->Trim(current_access_unit_pos_);

  return true;
}

bool EsParserH264::EmitFrame(int64_t access_unit_pos,
                             int access_unit_size,
                             bool is_key_frame,
                             int pps_id) {
  // Get the access unit timing info.
  // Note: |current_timing_desc.pts| might be |kNoTimestamp| at this point
  // if:
  // - the stream is not fully MPEG-2 compliant.
  // - or if the stream relies on H264 VUI parameters to compute the timestamps.
  //   See H.222 spec: section 2.7.5 "Conditional coding of timestamps".
  //   This part is not yet implemented in EsParserH264.
  // |es_adapter_| will take care of the missing timestamps.
  TimingDesc current_timing_desc = GetTimingDescriptor(access_unit_pos);
  DVLOG_IF(1, current_timing_desc.pts == kNoTimestamp) << "Missing timestamp";

  // If only the PTS is provided, copy the PTS into the DTS.
  if (current_timing_desc.dts == kNoDecodeTimestamp) {
    current_timing_desc.dts =
        DecodeTimestamp::FromPresentationTime(current_timing_desc.pts);
  }

  // Update the video decoder configuration if needed.
  const H264PPS* pps = h264_parser_->GetPPS(pps_id);
  if (!pps) {
    // Only accept an invalid PPS at the beginning when the stream
    // does not necessarily start with an SPS/PPS/IDR.
    // In this case, the initial frames are conveyed to the upper layer with
    // an invalid VideoDecoderConfig and it's up to the upper layer
    // to process this kind of frame accordingly.
    if (last_video_decoder_config_.IsValidConfig())
      return false;
  } else {
    const H264SPS* sps = h264_parser_->GetSPS(pps->seq_parameter_set_id);
    if (!sps)
      return false;
    RCHECK(UpdateVideoDecoderConfig(sps, init_encryption_scheme_));
  }

  // Emit a frame.
  DVLOG(LOG_LEVEL_ES) << "Emit frame: stream_pos=" << current_access_unit_pos_
                      << " size=" << access_unit_size;
  int es_size;
  const uint8_t* es;
  es_queue_->PeekAt(current_access_unit_pos_, &es, &es_size);
  CHECK_GE(es_size, access_unit_size);

  const DecryptConfig* base_decrypt_config = nullptr;
  if (get_decrypt_config_cb_)
    base_decrypt_config = get_decrypt_config_cb_.Run();

  std::unique_ptr<uint8_t[]> adjusted_au;
  std::vector<SubsampleEntry> subsamples;
  if (base_decrypt_config) {
    adjusted_au = AdjustAUForSampleAES(es, &access_unit_size, protected_blocks_,
                                       &subsamples);
    protected_blocks_.clear();
    if (adjusted_au)
      es = adjusted_au.get();
  }

  // TODO(wolenetz/acolwell): Validate and use a common cross-parser TrackId
  // type and allow multiple video tracks. See https://crbug.com/341581.
  scoped_refptr<StreamParserBuffer> stream_parser_buffer =
      StreamParserBuffer::CopyFrom(es, access_unit_size, is_key_frame,
                                   DemuxerStream::VIDEO, kMp2tVideoTrackId);
  stream_parser_buffer->SetDecodeTimestamp(current_timing_desc.dts);
  stream_parser_buffer->set_timestamp(current_timing_desc.pts);
  if (base_decrypt_config) {
    switch (base_decrypt_config->encryption_scheme()) {
      case EncryptionScheme::kUnencrypted:
        // As |base_decrypt_config| is specified, the stream is encrypted,
        // so this shouldn't happen.
        NOTREACHED_IN_MIGRATION();
        break;
      case EncryptionScheme::kCenc:
        stream_parser_buffer->set_decrypt_config(
            DecryptConfig::CreateCencConfig(base_decrypt_config->key_id(),
                                            base_decrypt_config->iv(),
                                            subsamples));
        break;
      case EncryptionScheme::kCbcs:
        // Note that for SampleAES the (encrypt,skip) pattern is constant.
        // If not specified in |base_decrypt_config|, use default values.
        stream_parser_buffer->set_decrypt_config(
            DecryptConfig::CreateCbcsConfig(
                base_decrypt_config->key_id(), base_decrypt_config->iv(),
                subsamples,
                EncryptionPattern(kSampleAESEncryptBlocks,
                                  kSampleAESSkipBlocks)));
        break;
    }
  }
  return es_adapter_.OnNewBuffer(stream_parser_buffer);
}

bool EsParserH264::UpdateVideoDecoderConfig(const H264SPS* sps,
                                            EncryptionScheme scheme) {
  // Set the SAR to 1 when not specified in the H264 stream.
  int sar_width = (sps->sar_width == 0) ? 1 : sps->sar_width;
  int sar_height = (sps->sar_height == 0) ? 1 : sps->sar_height;

  std::optional<gfx::Size> coded_size = sps->GetCodedSize();
  if (!coded_size)
    return false;

  std::optional<gfx::Rect> visible_rect = sps->GetVisibleRect();
  if (!visible_rect)
    return false;

  if (visible_rect->width() > std::numeric_limits<int>::max() / sar_width) {
    DVLOG(1) << "Integer overflow detected: visible_rect.width()="
             << visible_rect->width() << " sar_width=" << sar_width;
    return false;
  }
  gfx::Size natural_size((visible_rect->width() * sar_width) / sar_height,
                         visible_rect->height());
  if (natural_size.width() == 0)
    return false;

  VideoCodecProfile profile =
      H264Parser::ProfileIDCToVideoCodecProfile(sps->profile_idc);
  if (profile == VIDEO_CODEC_PROFILE_UNKNOWN) {
    DVLOG(1) << "Unrecognized SPS profile_idc 0x" << std::hex
             << sps->profile_idc;
    return false;
  }

  VideoDecoderConfig video_decoder_config(
      VideoCodec::kH264, profile, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace::REC709(), kNoTransformation, coded_size.value(),
      visible_rect.value(), natural_size, EmptyExtraData(), scheme);

  if (!video_decoder_config.IsValidConfig()) {
    DVLOG(1) << "Invalid video config: "
             << video_decoder_config.AsHumanReadableString();
    return false;
  }

  if (!video_decoder_config.Matches(last_video_decoder_config_)) {
    DVLOG(1) << "Profile IDC: " << sps->profile_idc;
    DVLOG(1) << "Level IDC: " << sps->level_idc;
    DVLOG(1) << "Pic width: " << coded_size->width();
    DVLOG(1) << "Pic height: " << coded_size->height();
    DVLOG(1) << "log2_max_frame_num_minus4: "
             << sps->log2_max_frame_num_minus4;
    DVLOG(1) << "SAR: width=" << sps->sar_width
             << " height=" << sps->sar_height;
    last_video_decoder_config_ = video_decoder_config;
    es_adapter_.OnConfigChanged(video_decoder_config);
  }

  return true;
}

}  // namespace mp2t
}  // namespace media
