// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_MP2T_STREAM_PARSER_H_
#define MEDIA_FORMATS_MP2T_MP2T_STREAM_PARSER_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <set>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/byte_queue.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"
#include "media/base/video_decoder_config.h"
#include "media/formats/mp2t/timestamp_unroller.h"
#include "media/media_buildflags.h"

namespace media {

class StreamParserBuffer;

namespace mp2t {

class Descriptors;
class EsParser;
class PidState;

class MEDIA_EXPORT Mp2tStreamParser : public StreamParser {
 public:
  explicit Mp2tStreamParser(
      std::optional<base::span<const std::string>> allowed_codecs,
      bool sbr_in_mimetype);

  Mp2tStreamParser(const Mp2tStreamParser&) = delete;
  Mp2tStreamParser& operator=(const Mp2tStreamParser&) = delete;

  ~Mp2tStreamParser() override;

  // StreamParser implementation.
  void Init(InitCB init_cb,
            NewConfigCB config_cb,
            NewBuffersCB new_buffers_cb,
            EncryptedMediaInitDataCB encrypted_media_init_data_cb,
            NewMediaSegmentCB new_segment_cb,
            EndMediaSegmentCB end_of_segment_cb,
            MediaLog* media_log) override;
  void Flush() override;
  bool GetGenerateTimestampsFlag() const override;
  [[nodiscard]] bool AppendToParseBuffer(
      base::span<const uint8_t> buf) override;
  [[nodiscard]] ParseStatus Parse(int max_pending_bytes_to_inspect) override;

 private:
  struct BufferQueueWithConfig {
    BufferQueueWithConfig(bool is_cfg_sent,
                          const AudioDecoderConfig& audio_cfg,
                          const VideoDecoderConfig& video_cfg);
    BufferQueueWithConfig(const BufferQueueWithConfig& other);
    ~BufferQueueWithConfig();

    bool is_config_sent;
    AudioDecoderConfig audio_config;
    StreamParser::BufferQueue audio_queue;
    VideoDecoderConfig video_config;
    StreamParser::BufferQueue video_queue;
  };

  // Callback invoked to register a Program Map Table.
  // Note: Does nothing if the PID is already registered.
  void RegisterPmt(int program_number, int pmt_pid);

  // Callback invoked to register a PES pid.
  // Possible values for |stream_type| are defined in:
  // ISO-13818.1 / ITU H.222 Table 2.34 "Stream type assignments".
  // |pes_pid| is part of the Program Map Table.
  // Some stream types are qualified by additional |descriptors|.
  void RegisterPes(int pes_pid,
                   int stream_type,
                   const Descriptors& descriptors);

  // Since the StreamParser interface allows only one audio & video streams,
  // an automatic PID filtering should be applied to select the audio & video
  // streams.
  void UpdatePidFilter();

  // Callback invoked each time the audio/video decoder configuration is
  // changed.
  void OnVideoConfigChanged(int pes_pid,
                            const VideoDecoderConfig& video_decoder_config);
  void OnAudioConfigChanged(int pes_pid,
                            const AudioDecoderConfig& audio_decoder_config);

  // Invoke the initialization callback if needed.
  bool FinishInitializationIfNeeded();

  // Callback invoked by the ES stream parser
  // to emit a new audio/video access unit.
  void OnEmitAudioBuffer(
      int pes_pid,
      scoped_refptr<StreamParserBuffer> stream_parser_buffer);
  void OnEmitVideoBuffer(
      int pes_pid,
      scoped_refptr<StreamParserBuffer> stream_parser_buffer);
  bool EmitRemainingBuffers();

  std::unique_ptr<EsParser> CreateH264Parser(int pes_pid);
  std::unique_ptr<EsParser> CreateAacParser(int pes_pid);
  std::unique_ptr<EsParser> CreateMpeg1AudioParser(int pes_pid);

  bool ShouldForceEncryptedParser();
  std::unique_ptr<EsParser> CreateEncryptedH264Parser(int pes_pid,
                                                      bool emit_clear_buffers);
  std::unique_ptr<EsParser> CreateEncryptedAacParser(int pes_pid,
                                                     bool emit_clear_buffers);

  std::unique_ptr<PidState> MakeCatPidState();
  void UnregisterCat();

  // Register the PIDs for the Cenc packets (CENC-ECM and CENC-PSSH).
  void RegisterCencPids(int ca_pid, int pssh_pid);
  void UnregisterCencPids();

  // Register a default encryption mode to be used for decoder configs. This
  // value is only used in the absence of explicit encryption metadata, as might
  // be the case during an unencrypted portion of a live stream.
  void RegisterEncryptionScheme(EncryptionScheme scheme);

  // Register the new KeyID and IV (parsed from CENC-ECM).
  void RegisterNewKeyIdAndIv(const std::string& key_id, const std::string& iv);

  // Register the PSSH (parsed from CENC-PSSH).
  void RegisterPsshBoxes(const std::vector<uint8_t>& init_data);

  const DecryptConfig* GetDecryptConfig() { return decrypt_config_.get(); }

  // List of callbacks.
  InitCB init_cb_;
  NewConfigCB config_cb_;
  NewBuffersCB new_buffers_cb_;
  EncryptedMediaInitDataCB encrypted_media_init_data_cb_;
  NewMediaSegmentCB new_segment_cb_;
  EndMediaSegmentCB end_of_segment_cb_;
  raw_ptr<MediaLog> media_log_;

  // List of allowed stream types for this parser. If this set is `nullopt`,
  // allowed stream type checking is disabled. An empty set implies no codecs
  // are allowed.
  std::optional<base::flat_set<int>> allowed_stream_types_;

  // True when AAC SBR extension is signalled in the mimetype
  // (mp4a.40.5 in the codecs parameter).
  bool sbr_in_mimetype_;

  // Bytes of the TS stream.
  // `uninspected_pending_bytes_` tracks how much data has not yet been
  // attempted to be parsed from `ts_byte_queue_` between calls to Parse().
  // AppendToParseBuffer() increases this from 0 as more data is added. Parse()
  // incrementally reduces this and Flush() zeroes this. Note that Parse() may
  // have inspected some data at the front of `ts_byte_queue_` but not yet been
  // able to pop it from the queue. So this value may be lower than the actual
  // amount of bytes in `ts_byte_queue_`, since more data is needed to complete
  // the parse.
  size_t uninspected_pending_bytes_ = 0;
  ByteQueue ts_byte_queue_;

  // List of PIDs and their state.
  std::map<int, std::unique_ptr<PidState>> pids_;

  // Selected audio and video PIDs.
  int selected_audio_pid_;
  int selected_video_pid_;

  // Pending audio & video buffers.
  std::list<BufferQueueWithConfig> buffer_queue_chain_;

  // Whether |init_cb_| has been invoked.
  bool is_initialized_;

  // Indicate whether a segment was started.
  bool segment_started_;

  // Timestamp unroller.
  // Timestamps in PES packets must be unrolled using the same offset.
  // So the unroller is global between PES pids.
  TimestampUnroller timestamp_unroller_;

  EncryptionScheme initial_encryption_scheme_ = EncryptionScheme::kUnencrypted;

  // TODO(jrummell): Rather than store the key_id and iv in a DecryptConfig,
  // provide a better way to access the last values seen in a ECM packet.
  std::unique_ptr<DecryptConfig> decrypt_config_;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_MP2T_STREAM_PARSER_H_
