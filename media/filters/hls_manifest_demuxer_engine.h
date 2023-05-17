// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_MANIFEST_DEMUXER_ENGINE_H_
#define MEDIA_FILTERS_HLS_MANIFEST_DEMUXER_ENGINE_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/pipeline_status.h"
#include "media/filters/hls_codec_detector.h"
#include "media/filters/hls_data_source_provider.h"
#include "media/filters/hls_demuxer_status.h"
#include "media/filters/hls_rendition.h"
#include "media/filters/manifest_demuxer.h"
#include "media/formats/hls/media_playlist.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/rendition_selector.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

// A HLS-Parser/Player implementation of ManifestDemuxer's Engine interface.
// This will use the HLS parsers and rendition selectors to fetch and parse
// playlists, followed by fetching and appending media segments.
class MEDIA_EXPORT HlsManifestDemuxerEngine : public ManifestDemuxer::Engine,
                                              public HlsRenditionHost {
 public:
  HlsManifestDemuxerEngine(base::SequenceBound<HlsDataSourceProvider> dsp,
                           scoped_refptr<base::SequencedTaskRunner> task_runner,
                           GURL root_playlist_uri,
                           MediaLog* media_log);
  ~HlsManifestDemuxerEngine() override;

  std::string GetName() const override;
  void Initialize(ManifestDemuxerEngineHost* host,
                  PipelineStatusCallback status_cb) override;
  void OnTimeUpdate(base::TimeDelta time,
                    double playback_rate,
                    ManifestDemuxer::DelayCallback cb) override;
  bool Seek(base::TimeDelta time) override;
  void StartWaitingForSeek() override;
  void AbortPendingReads() override;
  bool IsSeekable() override;
  int64_t GetMemoryUsage() const override;
  void Stop() override;
  void ReadFromUrl(GURL uri,
                   bool read_chunked,
                   absl::optional<hls::types::ByteRange> range,
                   HlsDataSourceStream::ReadCb cb) override;

  // Parses a playlist using the multivariant playlist, if it's being used.
  hls::ParseStatus::Or<scoped_refptr<hls::MediaPlaylist>>
  ParseMediaPlaylistFromStream(HlsDataSourceStream stream,
                               GURL uri,
                               hls::types::DecimalInteger version);

 private:
  struct PlaylistParseInfo {
    PlaylistParseInfo(GURL uri,
                      std::vector<std::string> codecs,
                      std::string role,
                      bool allow_multivariant_playlist = false);
    PlaylistParseInfo(const PlaylistParseInfo& copy);
    ~PlaylistParseInfo();

    // The url that this media playlist came from. We might need to update it
    // if its a live playlist, so it's vital to keep it around.
    GURL uri;

    // Any detected codecs associated with this stream.
    std::vector<std::string> codecs;

    // The name given to this stream in chunk demuxer.
    std::string role;

    // Only root playlists are allowed to be multivariant.
    bool allow_multivariant_playlist;
  };

  // Helpers to call |PlayerImplDemuxer::OnDemuxerError|.
  void Abort(HlsDemuxerStatus status);
  void Abort(hls::ParseStatus status);
  void Abort(HlsDataSource::ReadStatus status);

  // ReadFromUrl needs a helper to abort in case of failure.
  void ReadDataSource(bool read_chunked,
                      HlsDataSourceStream::ReadCb cb,
                      std::unique_ptr<HlsDataSource> source);

  void ParsePlaylist(PipelineStatusCallback parse_complete_cb,
                     PlaylistParseInfo parse_info,
                     HlsDataSourceStream::ReadResult m_stream);

  void OnMultivariantPlaylist(
      PipelineStatusCallback parse_complete_cb,
      scoped_refptr<hls::MultivariantPlaylist> playlist);
  void SetStreams(std::vector<PlaylistParseInfo> playlists,
                  PipelineStatusCallback cb,
                  PipelineStatus exit_on_error);

  void OnMediaPlaylist(PipelineStatusCallback parse_complete_cb,
                       PlaylistParseInfo parse_info,
                       scoped_refptr<hls::MediaPlaylist> playlist);
  void DetermineStreamContainerAndCodecs(
      hls::MediaPlaylist* playlist,
      PlaylistParseInfo parse_info,
      HlsDemuxerStatusCb<HlsCodecDetector::ContainerAndCodecs> container_cb);
  void OnPlaylistContainerDetermined(
      PipelineStatusCallback parse_complete_cb,
      PlaylistParseInfo parse_info,
      scoped_refptr<hls::MediaPlaylist> playlist,
      HlsDemuxerStatus::Or<HlsCodecDetector::ContainerAndCodecs> maybe_info);
  void PeekFirstSegment(
      PlaylistParseInfo parse_info,
      HlsDemuxerStatusCb<HlsCodecDetector::ContainerAndCodecs> cb,
      std::unique_ptr<HlsDataSource> data_source);

  void OnChunkDemuxerParseWarning(std::string role,
                                  SourceBufferParseWarning warning);
  void OnChunkDemuxerTracksChanged(std::string role,
                                   std::unique_ptr<MediaTracks> tracks);

  base::SequenceBound<HlsDataSourceProvider> data_source_provider_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // root playlist, either multivariant or media.
  GURL root_playlist_uri_;

  std::unique_ptr<MediaLog> media_log_;
  base::raw_ptr<ManifestDemuxerEngineHost> host_ = nullptr;

  // The codec detector is a reusable way for determining codecs in a media
  // stream.
  std::unique_ptr<HlsCodecDetector> codec_detector_;

  // If the root playlist is multivariant, we need to store it for parsing the
  // dependant media playlists.
  scoped_refptr<hls::MultivariantPlaylist> multivariant_root_;
  std::unique_ptr<hls::RenditionSelector> rendition_selector_;

  // Multiple renditions are allowed, and have to be synchronized.
  std::vector<std::unique_ptr<HlsRendition>> renditions_;

  // Preferences for selecting optimal renditions. Storing them allows them
  // to be changed later due to network constraints or user changes.
  hls::RenditionSelector::VideoPlaybackPreferences video_preferences_ = {
      absl::nullopt, absl::nullopt};
  hls::RenditionSelector::AudioPlaybackPreferences audio_preferences_ = {
      absl::nullopt, absl::nullopt};

  base::WeakPtrFactory<HlsManifestDemuxerEngine> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_MANIFEST_DEMUXER_ENGINE_H_
