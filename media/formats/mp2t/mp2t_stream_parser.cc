// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp2t/mp2t_stream_parser.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/checked_math.h"
#include "media/base/byte_queue.h"
#include "media/base/media_tracks.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_codec_string_parsers.h"
#include "media/formats/mp2t/descriptors.h"
#include "media/formats/mp2t/es_parser.h"
#include "media/formats/mp2t/es_parser_adts.h"
#include "media/formats/mp2t/es_parser_h264.h"
#include "media/formats/mp2t/es_parser_mpeg1audio.h"
#include "media/formats/mp2t/mp2t_common.h"
#include "media/formats/mp2t/ts_packet.h"
#include "media/formats/mp2t/ts_section.h"
#include "media/formats/mp2t/ts_section_cat.h"
#include "media/formats/mp2t/ts_section_cets_ecm.h"
#include "media/formats/mp2t/ts_section_cets_pssh.h"
#include "media/formats/mp2t/ts_section_pat.h"
#include "media/formats/mp2t/ts_section_pes.h"
#include "media/formats/mp2t/ts_section_pmt.h"

namespace media {
namespace mp2t {

namespace {

enum StreamType {
  // ISO-13818.1 / ITU H.222 Table 2.34 "Stream type assignments"
  kStreamTypeMpeg1Audio = 0x3,
  // ISO/IEC 13818-3 Audio (MPEG-2)
  kStreamTypeMpeg2Audio = 0x4,
  kStreamTypeAAC = 0xf,
  kStreamTypeAVC = 0x1b,
  kStreamTypeAACWithSampleAES = 0xcf,
  kStreamTypeAVCWithSampleAES = 0xdb,
  // TODO(dougsteed). Consider adding support for the following:
  //  kStreamTypeAC3WithSampleAES = 0xc1,
  //  kStreamTypeEAC3WithSampleAES = 0xc2,
};

constexpr int64_t kSampleAESPrivateDataIndicatorAVC = 0x7a617663;
constexpr int64_t kSampleAESPrivateDataIndicatorAAC = 0x61616364;
// TODO(dougsteed). Consider adding support for the following:
// const int64_t kSampleAESPrivateDataIndicatorAC3 = 0x61633364;
// const int64_t kSampleAESPrivateDataIndicatorEAC3 = 0x65633364;

std::optional<base::flat_set<int>> MapAllowedStreamTypes(
    std::optional<base::span<const std::string>> allowed_codecs) {
  if (!allowed_codecs.has_value()) {
    return std::nullopt;
  }
  base::flat_set<int> allowed_stream_types;
  for (const std::string& codec_name : *allowed_codecs) {
    switch (StringToVideoCodec(codec_name)) {
      case VideoCodec::kH264:
        allowed_stream_types.insert(kStreamTypeAVC);
        allowed_stream_types.insert(kStreamTypeAVCWithSampleAES);
        continue;
      case VideoCodec::kUnknown:
        // Probably audio.
        break;
      default:
        DLOG(WARNING) << "Unsupported video codec " << codec_name;
        continue;
    }

    switch (StringToAudioCodec(codec_name)) {
      case AudioCodec::kAAC:
        allowed_stream_types.insert(kStreamTypeAAC);
        allowed_stream_types.insert(kStreamTypeAACWithSampleAES);
        continue;
      case AudioCodec::kMP3:
        allowed_stream_types.insert(kStreamTypeMpeg1Audio);
        allowed_stream_types.insert(kStreamTypeMpeg2Audio);
        continue;
      case AudioCodec::kUnknown:
        // Neither audio, nor video.
        break;
      default:
        DLOG(WARNING) << "Unsupported audio codec " << codec_name;
        continue;
    }

    // Failed to parse as an audio or a video codec.
    DLOG(WARNING) << "Unknown codec " << codec_name;
  }
  return allowed_stream_types;
}

}  // namespace

class PidState {
 public:
  enum PidType {
    kPidPat,
    kPidPmt,
    kPidAudioPes,
    kPidVideoPes,
    kPidCat,
    kPidCetsEcm,
    kPidCetsPssh,
  };

  PidState(int pid,
           PidType pid_type,
           std::unique_ptr<TsSection> section_parser);

  // Extract the content of the TS packet and parse it.
  // Return true if successful.
  bool PushTsPacket(const TsPacket& ts_packet);

  // Flush the PID state (possibly emitting some pending frames)
  // and reset its state.
  void Flush();

  // Enable/disable the PID.
  // Disabling a PID will reset its state and ignore any further incoming TS
  // packets.
  void Enable();
  void Disable();
  bool IsEnabled() const;

  PidType pid_type() const { return pid_type_; }

 private:
  void ResetState();

  int pid_;
  PidType pid_type_;
  std::unique_ptr<TsSection> section_parser_;

  bool enable_;

  int continuity_counter_;
};

PidState::PidState(int pid,
                   PidType pid_type,
                   std::unique_ptr<TsSection> section_parser)
    : pid_(pid),
      pid_type_(pid_type),
      section_parser_(std::move(section_parser)),
      enable_(false),
      continuity_counter_(-1) {
  DCHECK(section_parser_);
}

bool PidState::PushTsPacket(const TsPacket& ts_packet) {
  DCHECK_EQ(ts_packet.pid(), pid_);

  // The current PID is not part of the PID filter,
  // just discard the incoming TS packet.
  if (!enable_)
    return true;

  int expected_continuity_counter = (continuity_counter_ + 1) % 16;
  if (continuity_counter_ >= 0 &&
      ts_packet.continuity_counter() != expected_continuity_counter) {
    DVLOG(1) << "TS discontinuity detected for pid: " << pid_;
    return false;
  }

  bool status = section_parser_->Parse(ts_packet.payload_unit_start_indicator(),
                                       ts_packet.payload());

  // At the minimum, when parsing failed, auto reset the section parser.
  // Components that use the StreamParser can take further action if needed.
  if (!status) {
    DVLOG(1) << "Parsing failed for pid = " << pid_;
    ResetState();
  }

  return status;
}

void PidState::Flush() {
  section_parser_->Flush();
  ResetState();
}

void PidState::Enable() {
  enable_ = true;
}

void PidState::Disable() {
  if (!enable_)
    return;

  ResetState();
  enable_ = false;
}

bool PidState::IsEnabled() const {
  return enable_;
}

void PidState::ResetState() {
  section_parser_->Reset();
  continuity_counter_ = -1;
}

Mp2tStreamParser::BufferQueueWithConfig::BufferQueueWithConfig(
    bool is_cfg_sent,
    const AudioDecoderConfig& audio_cfg,
    const VideoDecoderConfig& video_cfg)
  : is_config_sent(is_cfg_sent),
    audio_config(audio_cfg),
    video_config(video_cfg) {
}

Mp2tStreamParser::BufferQueueWithConfig::BufferQueueWithConfig(
    const BufferQueueWithConfig& other) = default;

Mp2tStreamParser::BufferQueueWithConfig::~BufferQueueWithConfig() {
}

Mp2tStreamParser::Mp2tStreamParser(
    std::optional<base::span<const std::string>> allowed_codecs,
    bool sbr_in_mimetype)
    : allowed_stream_types_(MapAllowedStreamTypes(allowed_codecs)),
      sbr_in_mimetype_(sbr_in_mimetype),
      selected_audio_pid_(-1),
      selected_video_pid_(-1),
      is_initialized_(false),
      segment_started_(false) {}

Mp2tStreamParser::~Mp2tStreamParser() = default;

void Mp2tStreamParser::Init(
    InitCB init_cb,
    NewConfigCB config_cb,
    NewBuffersCB new_buffers_cb,
    EncryptedMediaInitDataCB encrypted_media_init_data_cb,
    NewMediaSegmentCB new_segment_cb,
    EndMediaSegmentCB end_of_segment_cb,
    MediaLog* media_log) {
  DCHECK(!is_initialized_);
  DCHECK(!init_cb_);
  DCHECK(init_cb);
  DCHECK(config_cb);
  DCHECK(new_buffers_cb);
  DCHECK(encrypted_media_init_data_cb);
  DCHECK(new_segment_cb);
  DCHECK(end_of_segment_cb);

  init_cb_ = std::move(init_cb);
  config_cb_ = std::move(config_cb);
  new_buffers_cb_ = std::move(new_buffers_cb);
  encrypted_media_init_data_cb_ = std::move(encrypted_media_init_data_cb);
  new_segment_cb_ = std::move(new_segment_cb);
  end_of_segment_cb_ = std::move(end_of_segment_cb);
  media_log_ = media_log;
}

void Mp2tStreamParser::Flush() {
  DVLOG(1) << "Mp2tStreamParser::Flush";

  // Flush the buffers and reset the pids.
  for (const auto& pid_pair : pids_) {
    DVLOG(1) << "Flushing PID: " << pid_pair.first;
    pid_pair.second->Flush();
  }
  pids_.clear();

  // Flush is invoked from SourceBuffer.abort/SourceState::ResetParserState, and
  // MSE spec prohibits emitting new configs in ResetParserState algorithm (see
  // https://w3c.github.io/media-source/#sourcebuffer-reset-parser-state,
  // 3.5.2 Reset Parser State states that new frames might be processed only in
  // PARSING_MEDIA_SEGMENT and therefore doesn't allow emitting new configs,
  // since that might need to run "init segment received" algorithm).
  // So before we emit remaining buffers here, we need to trim our buffer queue
  // so that we leave only buffers with configs that were already sent.
  for (auto buffer_queue_iter = buffer_queue_chain_.begin();
       buffer_queue_iter != buffer_queue_chain_.end(); ++buffer_queue_iter) {
    const BufferQueueWithConfig& queue_with_config = *buffer_queue_iter;
    if (!queue_with_config.is_config_sent) {
      DVLOG(LOG_LEVEL_ES) << "Flush: dropping buffers with unsent new configs.";
      buffer_queue_chain_.erase(buffer_queue_iter, buffer_queue_chain_.end());
      break;
    }
  }

  EmitRemainingBuffers();
  buffer_queue_chain_.clear();

  // End of the segment.
  // Note: does not need to invoke |end_of_segment_cb_| since flushing the
  // stream parser already involves the end of the current segment.
  segment_started_ = false;

  // Remove any bytes left in the TS buffer.
  // (i.e. any partial TS packet => less than 188 bytes).
  ts_byte_queue_.Reset();
  uninspected_pending_bytes_ = 0;

  // Reset the selected PIDs.
  selected_audio_pid_ = -1;
  selected_video_pid_ = -1;

  // Reset the timestamp unroller.
  timestamp_unroller_.Reset();
}

bool Mp2tStreamParser::GetGenerateTimestampsFlag() const {
  return false;
}

bool Mp2tStreamParser::AppendToParseBuffer(base::span<const uint8_t> buf) {
  DVLOG(1) << __func__ << " size=" << buf.size();

  // Ensure that we are not still in the middle of iterating Parse calls for
  // previously appended data. May consider changing this to a DCHECK once
  // stabilized, though since impact of proceeding when this condition fails
  // could lead to memory corruption, preferring CHECK.
  CHECK_EQ(uninspected_pending_bytes_, 0u);

  // Add the data to the parser state.
  uninspected_pending_bytes_ = base::checked_cast<int>(buf.size());
  if (!ts_byte_queue_.Push(buf)) {
    DVLOG(2) << "AppendToParseBuffer(): Failed to push buf of size "
             << buf.size();
    return false;
  }

  return true;
}

StreamParser::ParseStatus Mp2tStreamParser::Parse(
    int max_pending_bytes_to_inspect) {
  DVLOG(1) << __func__;
  DCHECK_GE(max_pending_bytes_to_inspect, 0);

  auto queue_data = ts_byte_queue_.Data();
  const uint8_t* ts_buffer = queue_data.data();
  size_t queue_size = queue_data.size();
  CHECK_GE(queue_size, uninspected_pending_bytes_);

  // First, determine the amount of bytes not yet popped, though already
  // inspected by previous call(s) to Parse().
  size_t ts_buffer_size = queue_size - uninspected_pending_bytes_;

  // Next, allow up to `max_pending_bytes_to_inspect` more of `queue_` contents
  // beyond those previously inspected to be involved in this Parse() call.
  int inspection_increment =
      std::min(base::checked_cast<size_t>(max_pending_bytes_to_inspect),
               uninspected_pending_bytes_);
  ts_buffer_size += inspection_increment;

  // If successfully parsed, remember that we will have inspected this
  // incremental part of `ts_byte_queue_` contents. Note that parse failures are
  // fatal.
  uninspected_pending_bytes_ -= inspection_increment;
  DCHECK_GE(uninspected_pending_bytes_, 0u);

  int bytes_to_pop = 0;

  while (true) {
    if (ts_buffer_size < TsPacket::kPacketSize) {
      break;
    }

    // Synchronization.
    size_t skipped_bytes = TsPacket::Sync(ts_buffer, ts_buffer_size);
    if (skipped_bytes > 0) {
      DVLOG(1) << "Packet not aligned on a TS syncword:"
               << " skipped_bytes=" << skipped_bytes;
      CHECK_GE(ts_buffer_size, skipped_bytes);
      ts_buffer_size -= skipped_bytes;
      ts_buffer += skipped_bytes;
      bytes_to_pop += skipped_bytes;
      continue;
    }

    // Parse the TS header, skipping 1 byte if the header is invalid.
    std::unique_ptr<TsPacket> ts_packet(
        TsPacket::Parse(ts_buffer, ts_buffer_size));
    if (!ts_packet) {
      DVLOG(1) << "Error: invalid TS packet";
      CHECK_GE(ts_buffer_size, 1u);
      ts_buffer_size--;
      ts_buffer++;
      bytes_to_pop++;
      continue;
    }
    DVLOG(LOG_LEVEL_TS)
        << "Processing PID=" << ts_packet->pid()
        << " start_unit=" << ts_packet->payload_unit_start_indicator();

    // Parse the section.
    auto it = pids_.find(ts_packet->pid());
    if (it == pids_.end() &&
        ts_packet->pid() == TsSection::kPidPat) {
      // Create the PAT state here if needed.
      auto pat_section_parser =
          std::make_unique<TsSectionPat>(base::BindRepeating(
              &Mp2tStreamParser::RegisterPmt, base::Unretained(this)));
      auto pat_pid_state = std::make_unique<PidState>(
          ts_packet->pid(), PidState::kPidPat, std::move(pat_section_parser));
      pat_pid_state->Enable();
      it = pids_
               .insert(
                   std::make_pair(ts_packet->pid(), std::move(pat_pid_state)))
               .first;
    } else if (it == pids_.end() && ts_packet->pid() == TsSection::kPidCat) {
      // We allow a CAT to appear as the first packet in the TS. This allows us
      // to specify encryption metadata for HLS by injecting it as an extra TS
      // packet at the front of the stream.

      it = pids_.insert(std::make_pair(TsSection::kPidCat, MakeCatPidState()))
               .first;
    }

    if (it != pids_.end()) {
      if (!it->second->PushTsPacket(*ts_packet))
        return ParseStatus::kFailed;
    } else {
      DVLOG(LOG_LEVEL_TS) << "Ignoring TS packet for pid: " << ts_packet->pid();
    }

    // Go to the next packet.
    ts_buffer_size -= TsPacket::kPacketSize;
    ts_buffer += TsPacket::kPacketSize;
    bytes_to_pop += TsPacket::kPacketSize;
  }

  if (!FinishInitializationIfNeeded()) {
    // Inlining a former RCHECK here, since we cannot return false from this
    // method any longer.
    DLOG(WARNING)
        << "Failure while parsing Mpeg2TS: FinishInitializationIfNeeded()";
    return ParseStatus::kFailed;
  }

  // Emit the A/V buffers that kept accumulating during TS parsing.
  if (!EmitRemainingBuffers()) {
    return ParseStatus::kFailed;
  }

  ts_byte_queue_.Pop(bytes_to_pop);
  if (uninspected_pending_bytes_ > 0) {
    return ParseStatus::kSuccessHasMoreData;
  }
  return ParseStatus::kSuccess;
}

void Mp2tStreamParser::RegisterPmt(int program_number, int pmt_pid) {
  DVLOG(1) << "RegisterPmt:"
           << " program_number=" << program_number
           << " pmt_pid=" << pmt_pid;

  // Only one TS program is allowed. Ignore the incoming program map table,
  // if there is already one registered.
  for (const auto& pid_pair : pids_) {
    PidState* pid_state = pid_pair.second.get();
    if (pid_state->pid_type() == PidState::kPidPmt) {
      DVLOG_IF(1, pmt_pid != pid_pair.first)
          << "More than one program is defined";
      return;
    }
  }

  // Create the PMT state here if needed.
  DVLOG(1) << "Create a new PMT parser";
  auto pmt_section_parser = std::make_unique<TsSectionPmt>(base::BindRepeating(
      &Mp2tStreamParser::RegisterPes, base::Unretained(this)));
  auto pmt_pid_state = std::make_unique<PidState>(
      pmt_pid, PidState::kPidPmt, std::move(pmt_section_parser));
  pmt_pid_state->Enable();
  pids_.insert(std::make_pair(pmt_pid, std::move(pmt_pid_state)));

  // Take the opportunity to clean up any PIDs that were involved in importing
  // encryption metadata for HLS with SampleAES. This prevents the possibility
  // of interference with actual PIDs that might be declared in the PMT.
  // TODO(dougsteed): if in the future the appropriate PIDs are embedded in the
  // source stream, this will not be necessary.
  UnregisterCat();
  UnregisterCencPids();
}

std::unique_ptr<EsParser> Mp2tStreamParser::CreateH264Parser(int pes_pid) {
  auto on_video_config_changed = base::BindRepeating(
      &Mp2tStreamParser::OnVideoConfigChanged, base::Unretained(this), pes_pid);
  auto on_emit_video_buffer = base::BindRepeating(
      &Mp2tStreamParser::OnEmitVideoBuffer, base::Unretained(this), pes_pid);

  return std::make_unique<EsParserH264>(std::move(on_video_config_changed),
                                        std::move(on_emit_video_buffer));
}

std::unique_ptr<EsParser> Mp2tStreamParser::CreateAacParser(int pes_pid) {
  auto on_audio_config_changed = base::BindRepeating(
      &Mp2tStreamParser::OnAudioConfigChanged, base::Unretained(this), pes_pid);
  auto on_emit_audio_buffer = base::BindRepeating(
      &Mp2tStreamParser::OnEmitAudioBuffer, base::Unretained(this), pes_pid);
  return std::make_unique<EsParserAdts>(on_audio_config_changed,
                                        std::move(on_emit_audio_buffer),
                                        sbr_in_mimetype_);
}

std::unique_ptr<EsParser> Mp2tStreamParser::CreateMpeg1AudioParser(
    int pes_pid) {
  auto on_audio_config_changed = base::BindRepeating(
      &Mp2tStreamParser::OnAudioConfigChanged, base::Unretained(this), pes_pid);
  auto on_emit_audio_buffer = base::BindRepeating(
      &Mp2tStreamParser::OnEmitAudioBuffer, base::Unretained(this), pes_pid);
  return std::make_unique<EsParserMpeg1Audio>(
      on_audio_config_changed, std::move(on_emit_audio_buffer), media_log_);
}

bool Mp2tStreamParser::ShouldForceEncryptedParser() {
  // If we expect to handle encrypted data later in the stream, then force the
  // use of the encrypted parser variant so that the initial configuration
  // reflects the intended encryption mode (even if the initial segment itself
  // is not encrypted).
  return initial_encryption_scheme_ != EncryptionScheme::kUnencrypted;
}

std::unique_ptr<EsParser> Mp2tStreamParser::CreateEncryptedH264Parser(
    int pes_pid,
    bool emit_clear_buffers) {
  auto on_video_config_changed = base::BindRepeating(
      &Mp2tStreamParser::OnVideoConfigChanged, base::Unretained(this), pes_pid);
  auto on_emit_video_buffer = base::BindRepeating(
      &Mp2tStreamParser::OnEmitVideoBuffer, base::Unretained(this), pes_pid);
  EsParserAdts::GetDecryptConfigCB get_decrypt_config;
  if (!emit_clear_buffers) {
    get_decrypt_config = base::BindRepeating(
        &Mp2tStreamParser::GetDecryptConfig, base::Unretained(this));
  }
  return std::make_unique<EsParserH264>(
      std::move(on_video_config_changed), on_emit_video_buffer,
      initial_encryption_scheme_, std::move(get_decrypt_config));
}

std::unique_ptr<EsParser> Mp2tStreamParser::CreateEncryptedAacParser(
    int pes_pid,
    bool emit_clear_buffers) {
  auto on_audio_config_changed = base::BindRepeating(
      &Mp2tStreamParser::OnAudioConfigChanged, base::Unretained(this), pes_pid);
  auto on_emit_audio_buffer = base::BindRepeating(
      &Mp2tStreamParser::OnEmitAudioBuffer, base::Unretained(this), pes_pid);
  EsParserAdts::GetDecryptConfigCB get_decrypt_config;
  if (!emit_clear_buffers) {
    get_decrypt_config = base::BindRepeating(
        &Mp2tStreamParser::GetDecryptConfig, base::Unretained(this));
  }
  return std::make_unique<EsParserAdts>(
      on_audio_config_changed, std::move(on_emit_audio_buffer),
      std::move(get_decrypt_config), initial_encryption_scheme_,
      sbr_in_mimetype_);
}

void Mp2tStreamParser::RegisterPes(int pes_pid,
                                   int stream_type,
                                   const Descriptors& descriptors) {
  // TODO(damienv): check there is no mismatch if the entry already exists.
  DVLOG(1) << "RegisterPes:"
           << " pes_pid=" << pes_pid
           << " stream_type=" << std::hex << stream_type << std::dec;
  auto it = pids_.find(pes_pid);
  if (it != pids_.end())
    return;

  // Ignore stream types not specified in the creation of the SourceBuffer.
  // See https://crbug.com/1169393.
  // TODO(crbug.com/41204005): Remove this hack when MSE stream/mime type
  // checks have been relaxed.
  if (allowed_stream_types_.has_value() &&
      !allowed_stream_types_->contains(stream_type)) {
    DVLOG(1) << "Stream type not allowed for this parser: " << stream_type;
    return;
  }

  // Create a stream parser corresponding to the stream type.
  bool is_audio = true;
  std::unique_ptr<EsParser> es_parser;

  switch (stream_type) {
    case kStreamTypeAVC:
      is_audio = false;
      if (ShouldForceEncryptedParser()) {
        es_parser =
            CreateEncryptedH264Parser(pes_pid, true /* emit_clear_buffers */);
        break;
      }
      es_parser = CreateH264Parser(pes_pid);
      break;

    case kStreamTypeAAC:
      if (ShouldForceEncryptedParser()) {
        es_parser =
            CreateEncryptedAacParser(pes_pid, true /* emit_clear_buffers */);
        break;
      }
      es_parser = CreateAacParser(pes_pid);
      break;

    case kStreamTypeMpeg1Audio:
    case kStreamTypeMpeg2Audio:
      es_parser = CreateMpeg1AudioParser(pes_pid);
      break;

    case kStreamTypeAVCWithSampleAES:
      if (descriptors.HasPrivateDataIndicator(
              kSampleAESPrivateDataIndicatorAVC)) {
        is_audio = false;
        es_parser =
            CreateEncryptedH264Parser(pes_pid, false /* emit_clear_buffers */);
      } else {
        VLOG(2) << "HLS: stream_type in PMT indicates AVC with Sample-AES, but "
                << "corresponding private data indicator is not present.";
      }
      break;

    case kStreamTypeAACWithSampleAES:
      if (descriptors.HasPrivateDataIndicator(
              kSampleAESPrivateDataIndicatorAAC)) {
        es_parser =
            CreateEncryptedAacParser(pes_pid, false /* emit_clear_buffers */);
      } else {
        VLOG(2) << "HLS: stream_type in PMT indicates AAC with Sample-AES, but "
                << "corresponding private data indicator is not present.";
      }
      break;

    default:
      // Unknown stream_type, so can't create a parser. Logged below.
      break;
  }

  if (!es_parser) {
    VLOG(1) << "Parser could not be created for stream_type: " << stream_type;
    return;
  }

  // Create the PES state here.
  DVLOG(1) << "Create a new PES state";
  auto pes_section_parser = std::make_unique<TsSectionPes>(
      std::move(es_parser), &timestamp_unroller_);
  PidState::PidType pid_type =
      is_audio ? PidState::kPidAudioPes : PidState::kPidVideoPes;
  auto pes_pid_state = std::make_unique<PidState>(
      pes_pid, pid_type, std::move(pes_section_parser));
  pids_.insert(std::make_pair(pes_pid, std::move(pes_pid_state)));

  // A new PES pid has been added, the PID filter might change.
  UpdatePidFilter();
}

void Mp2tStreamParser::UpdatePidFilter() {
  // Applies the HLS rule to select the default audio/video PIDs:
  // select the audio/video streams with the lowest PID.
  // TODO(damienv): this can be changed when the StreamParser interface
  // supports multiple audio/video streams.
  auto lowest_audio_pid = pids_.end();
  auto lowest_video_pid = pids_.end();
  for (auto it = pids_.begin(); it != pids_.end(); ++it) {
    int pid = it->first;
    PidState* pid_state = it->second.get();
    if (pid_state->pid_type() == PidState::kPidAudioPes &&
        (lowest_audio_pid == pids_.end() || pid < lowest_audio_pid->first))
      lowest_audio_pid = it;
    if (pid_state->pid_type() == PidState::kPidVideoPes &&
        (lowest_video_pid == pids_.end() || pid < lowest_video_pid->first))
      lowest_video_pid = it;
  }

  // Enable both the lowest audio and video PIDs.
  if (lowest_audio_pid != pids_.end()) {
    DVLOG(1) << "Enable audio pid: " << lowest_audio_pid->first;
    lowest_audio_pid->second->Enable();
    selected_audio_pid_ = lowest_audio_pid->first;
  }
  if (lowest_video_pid != pids_.end()) {
    DVLOG(1) << "Enable video pid: " << lowest_video_pid->first;
    lowest_video_pid->second->Enable();
    selected_video_pid_ = lowest_video_pid->first;
  }

  // Disable all the other audio and video PIDs.
  for (auto it = pids_.begin(); it != pids_.end(); ++it) {
    PidState* pid_state = it->second.get();
    if (it != lowest_audio_pid && it != lowest_video_pid &&
        (pid_state->pid_type() == PidState::kPidAudioPes ||
         pid_state->pid_type() == PidState::kPidVideoPes))
      pid_state->Disable();
  }
}

void Mp2tStreamParser::OnVideoConfigChanged(
    int pes_pid,
    const VideoDecoderConfig& video_decoder_config) {
  DVLOG(1) << "OnVideoConfigChanged for pid=" << pes_pid;
  DCHECK_EQ(pes_pid, selected_video_pid_);
  DCHECK(video_decoder_config.IsValidConfig());

  if (!buffer_queue_chain_.empty() &&
      !buffer_queue_chain_.back().video_config.IsValidConfig()) {
    // No video has been received so far, can reuse the existing video queue.
    DCHECK(buffer_queue_chain_.back().video_queue.empty());
    buffer_queue_chain_.back().video_config = video_decoder_config;
  } else {
    // Create a new entry in |buffer_queue_chain_| with the updated configs.
    BufferQueueWithConfig buffer_queue_with_config(
        false,
        buffer_queue_chain_.empty()
        ? AudioDecoderConfig() : buffer_queue_chain_.back().audio_config,
        video_decoder_config);
    buffer_queue_chain_.push_back(buffer_queue_with_config);
  }

  // Replace any non valid config with the 1st valid entry.
  // This might happen if there was no available config before.
  for (std::list<BufferQueueWithConfig>::iterator it =
       buffer_queue_chain_.begin(); it != buffer_queue_chain_.end(); ++it) {
    if (it->video_config.IsValidConfig())
      break;
    it->video_config = video_decoder_config;
  }
}

void Mp2tStreamParser::OnAudioConfigChanged(
    int pes_pid,
    const AudioDecoderConfig& audio_decoder_config) {
  DVLOG(1) << "OnAudioConfigChanged for pid=" << pes_pid;
  DCHECK_EQ(pes_pid, selected_audio_pid_);
  DCHECK(audio_decoder_config.IsValidConfig());

  if (!buffer_queue_chain_.empty() &&
      !buffer_queue_chain_.back().audio_config.IsValidConfig()) {
    // No audio has been received so far, can reuse the existing audio queue.
    DCHECK(buffer_queue_chain_.back().audio_queue.empty());
    buffer_queue_chain_.back().audio_config = audio_decoder_config;
  } else {
    // Create a new entry in |buffer_queue_chain_| with the updated configs.
    BufferQueueWithConfig buffer_queue_with_config(
        false,
        audio_decoder_config,
        buffer_queue_chain_.empty()
        ? VideoDecoderConfig() : buffer_queue_chain_.back().video_config);
    buffer_queue_chain_.push_back(buffer_queue_with_config);
  }

  // Replace any non valid config with the 1st valid entry.
  // This might happen if there was no available config before.
  for (std::list<BufferQueueWithConfig>::iterator it =
       buffer_queue_chain_.begin(); it != buffer_queue_chain_.end(); ++it) {
    if (it->audio_config.IsValidConfig())
      break;
    it->audio_config = audio_decoder_config;
  }
}

std::unique_ptr<MediaTracks> GenerateMediaTrackInfo(
    const AudioDecoderConfig& audio_config,
    const VideoDecoderConfig& video_config) {
  auto media_tracks = std::make_unique<MediaTracks>();
  // TODO(servolk): Implement proper sourcing of media track info as described
  // in crbug.com/590085
  if (audio_config.IsValidConfig()) {
    media_tracks->AddAudioTrack(audio_config, true, kMp2tAudioTrackId,
                                MediaTrack::Kind("main"), MediaTrack::Label(""),
                                MediaTrack::Language(""));
  }
  if (video_config.IsValidConfig()) {
    media_tracks->AddVideoTrack(video_config, true, kMp2tVideoTrackId,
                                MediaTrack::Kind("main"), MediaTrack::Label(""),
                                MediaTrack::Language(""));
  }
  return media_tracks;
}

bool Mp2tStreamParser::FinishInitializationIfNeeded() {
  // Nothing to be done if already initialized.
  if (is_initialized_)
    return true;

  // Wait for more data to come to finish initialization.
  if (buffer_queue_chain_.empty())
    return true;

  // Wait for more data to come if one of the config is not available.
  BufferQueueWithConfig& queue_with_config = buffer_queue_chain_.front();
  if (selected_audio_pid_ > 0 &&
      !queue_with_config.audio_config.IsValidConfig())
    return true;
  if (selected_video_pid_ > 0 &&
      !queue_with_config.video_config.IsValidConfig())
    return true;

  // Pass the config before invoking the initialization callback.
  std::unique_ptr<MediaTracks> media_tracks = GenerateMediaTrackInfo(
      queue_with_config.audio_config, queue_with_config.video_config);
  RCHECK(config_cb_.Run(std::move(media_tracks)));
  queue_with_config.is_config_sent = true;

  // For Mpeg2 TS, the duration is not known.
  DVLOG(1) << "Mpeg2TS stream parser initialization done";

  // TODO(wolenetz): If possible, detect and report track counts by type more
  // accurately here. Currently, capped at max 1 each for audio and video.
  InitParameters params(kInfiniteDuration);
  params.detected_audio_track_count =
      queue_with_config.audio_config.IsValidConfig() ? 1 : 0;
  params.detected_video_track_count =
      queue_with_config.video_config.IsValidConfig() ? 1 : 0;
  std::move(init_cb_).Run(params);
  is_initialized_ = true;

  return true;
}

void Mp2tStreamParser::OnEmitAudioBuffer(
    int pes_pid,
    scoped_refptr<StreamParserBuffer> stream_parser_buffer) {
  DCHECK_EQ(pes_pid, selected_audio_pid_);

  DVLOG(LOG_LEVEL_ES)
      << "OnEmitAudioBuffer: " << " size=" << stream_parser_buffer->size()
      << " dts=" << stream_parser_buffer->GetDecodeTimestamp().InMilliseconds()
      << " pts=" << stream_parser_buffer->timestamp().InMilliseconds()
      << " dur=" << stream_parser_buffer->duration().InMilliseconds();

  // Ignore the incoming buffer if it is not associated with any config.
  if (buffer_queue_chain_.empty()) {
    LOG(ERROR) << "Cannot provide buffers before configs";
    return;
  }

  buffer_queue_chain_.back().audio_queue.push_back(stream_parser_buffer);
}

void Mp2tStreamParser::OnEmitVideoBuffer(
    int pes_pid,
    scoped_refptr<StreamParserBuffer> stream_parser_buffer) {
  DCHECK_EQ(pes_pid, selected_video_pid_);

  DVLOG(LOG_LEVEL_ES)
      << "OnEmitVideoBuffer" << " size=" << stream_parser_buffer->size()
      << " dts=" << stream_parser_buffer->GetDecodeTimestamp().InMilliseconds()
      << " pts=" << stream_parser_buffer->timestamp().InMilliseconds()
      << " dur=" << stream_parser_buffer->duration().InMilliseconds()
      << " is_key_frame=" << stream_parser_buffer->is_key_frame();

  // Ignore the incoming buffer if it is not associated with any config.
  if (buffer_queue_chain_.empty()) {
    NOTREACHED_IN_MIGRATION() << "Cannot provide buffers before configs";
    return;
  }

  buffer_queue_chain_.back().video_queue.push_back(stream_parser_buffer);
}

bool Mp2tStreamParser::EmitRemainingBuffers() {
  DVLOG(LOG_LEVEL_ES) << "Mp2tStreamParser::EmitRemainingBuffers";

  // No buffer should be sent until fully initialized.
  if (!is_initialized_)
    return true;

  if (buffer_queue_chain_.empty())
    return true;

  // Keep track of the last audio and video config sent.
  AudioDecoderConfig last_audio_config =
      buffer_queue_chain_.back().audio_config;
  VideoDecoderConfig last_video_config =
      buffer_queue_chain_.back().video_config;

  // Do not have all the configs, need more data.
  if (selected_audio_pid_ >= 0 && !last_audio_config.IsValidConfig())
    return true;
  if (selected_video_pid_ >= 0 && !last_video_config.IsValidConfig())
    return true;

  // Buffer emission.
  while (!buffer_queue_chain_.empty()) {
    // Start a segment if needed.
    if (!segment_started_) {
      DVLOG(1) << "Starting a new segment";
      segment_started_ = true;
      new_segment_cb_.Run();
    }

    // Update the audio and video config if needed.
    BufferQueueWithConfig& queue_with_config = buffer_queue_chain_.front();
    if (!queue_with_config.is_config_sent) {
      std::unique_ptr<MediaTracks> media_tracks = GenerateMediaTrackInfo(
          queue_with_config.audio_config, queue_with_config.video_config);
      if (!config_cb_.Run(std::move(media_tracks))) {
        return false;
      }
      queue_with_config.is_config_sent = true;
    }

    // Add buffers.
    BufferQueueMap buffer_queue_map;
    if (!queue_with_config.audio_queue.empty())
      buffer_queue_map.insert(
          std::make_pair(kMp2tAudioTrackId, queue_with_config.audio_queue));
    if (!queue_with_config.video_queue.empty())
      buffer_queue_map.insert(
          std::make_pair(kMp2tVideoTrackId, queue_with_config.video_queue));

    if (!buffer_queue_map.empty() && !new_buffers_cb_.Run(buffer_queue_map))
      return false;

    buffer_queue_chain_.pop_front();
  }

  // Push an empty queue with the last audio/video config
  // so that buffers with the same config can be added later on.
  BufferQueueWithConfig queue_with_config(
      true, last_audio_config, last_video_config);
  buffer_queue_chain_.push_back(queue_with_config);

  return true;
}

std::unique_ptr<PidState> Mp2tStreamParser::MakeCatPidState() {
  auto cat_section_parser = std::make_unique<TsSectionCat>(
      base::BindRepeating(&Mp2tStreamParser::RegisterCencPids,
                          base::Unretained(this)),
      base::BindRepeating(&Mp2tStreamParser::RegisterEncryptionScheme,
                          base::Unretained(this)));
  auto cat_pid_state = std::make_unique<PidState>(
      TsSection::kPidCat, PidState::kPidCat, std::move(cat_section_parser));
  cat_pid_state->Enable();
  return cat_pid_state;
}

void Mp2tStreamParser::UnregisterCat() {
  for (auto& pid : pids_) {
    if (pid.second->pid_type() == PidState::kPidCat) {
      pids_.erase(pid.first);
      break;
    }
  }
}

void Mp2tStreamParser::RegisterCencPids(int ca_pid, int pssh_pid) {
  auto ecm_parser = std::make_unique<TsSectionCetsEcm>(base::BindRepeating(
      &Mp2tStreamParser::RegisterNewKeyIdAndIv, base::Unretained(this)));
  auto ecm_pid_state = std::make_unique<PidState>(ca_pid, PidState::kPidCetsEcm,
                                                  std::move(ecm_parser));
  ecm_pid_state->Enable();
  pids_.insert(std::make_pair(ca_pid, std::move(ecm_pid_state)));

  auto pssh_parser = std::make_unique<TsSectionCetsPssh>(base::BindRepeating(
      &Mp2tStreamParser::RegisterPsshBoxes, base::Unretained(this)));
  auto pssh_pid_state = std::make_unique<PidState>(
      pssh_pid, PidState::kPidCetsPssh, std::move(pssh_parser));
  pssh_pid_state->Enable();
  pids_.insert(std::make_pair(pssh_pid, std::move(pssh_pid_state)));
}

void Mp2tStreamParser::UnregisterCencPids() {
  for (auto& pid : pids_) {
    if (pid.second->pid_type() == PidState::kPidCetsEcm) {
      pids_.erase(pid.first);
      break;
    }
  }
  for (auto& pid : pids_) {
    if (pid.second->pid_type() == PidState::kPidCetsPssh) {
      pids_.erase(pid.first);
      break;
    }
  }
}

void Mp2tStreamParser::RegisterEncryptionScheme(EncryptionScheme scheme) {
  // We only need to record this for the initial decoder config.
  if (!is_initialized_) {
    initial_encryption_scheme_ = scheme;
  }
  // Reset the DecryptConfig, so that unless and until a CENC-ECM (containing
  // key id and IV) is seen, media data will be considered unencrypted. This is
  // similar to the way clear leaders can occur in MP4 containers.
  decrypt_config_.reset();
}

void Mp2tStreamParser::RegisterNewKeyIdAndIv(const std::string& key_id,
                                             const std::string& iv) {
  if (!iv.empty()) {
    switch (initial_encryption_scheme_) {
      case EncryptionScheme::kUnencrypted:
        decrypt_config_.reset();
        break;
      case EncryptionScheme::kCenc:
        decrypt_config_ = DecryptConfig::CreateCencConfig(key_id, iv, {});
        break;
      case EncryptionScheme::kCbcs:
        decrypt_config_ =
            DecryptConfig::CreateCbcsConfig(key_id, iv, {}, std::nullopt);
        break;
    }
  }
}

void Mp2tStreamParser::RegisterPsshBoxes(
    const std::vector<uint8_t>& init_data) {
  encrypted_media_init_data_cb_.Run(EmeInitDataType::CENC, init_data);
}

}  // namespace mp2t
}  // namespace media
