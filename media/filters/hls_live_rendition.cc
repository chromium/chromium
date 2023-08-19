// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_live_rendition.h"

#include "base/task/bind_post_task.h"
#include "media/filters/hls_manifest_demuxer_engine.h"

namespace media {

HlsLiveRendition::~HlsLiveRendition() {
  engine_host_->RemoveRole(role_);
}

HlsLiveRendition::HlsLiveRendition(ManifestDemuxerEngineHost* engine_host,
                                   HlsRenditionHost* rendition_host,
                                   std::string role,
                                   scoped_refptr<hls::MediaPlaylist> playlist,
                                   GURL media_playlist_uri)
    : engine_host_(engine_host),
      rendition_host_(rendition_host),
      role_(std::move(role)),
      media_playlist_uri_(std::move(media_playlist_uri)),
      segment_duration_upper_limit_(playlist->GetTargetDuration()) {
  AppendSegments(playlist.get());
}

absl::optional<base::TimeDelta> HlsLiveRendition::GetDuration() {
  return absl::nullopt;
}

void HlsLiveRendition::CheckState(
    base::TimeDelta media_time,
    double playback_rate,
    ManifestDemuxer::DelayCallback time_remaining_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (playback_rate != 1 && playback_rate != 0) {
    // TODO(crbug.com/1266991): What should be done about non-paused,
    // non-real-time playback? Anything above 1 would hit the end and constantly
    // be in a state of demuxer underflow, and anything slower than 1 would
    // eventually have so much data buffered that it would OOM.
    engine_host_->OnError(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  if (playback_rate != 0.0) {
    has_ever_played_ = true;
  }

  if (playback_rate == 0.0 && has_ever_played_) {
    require_seek_after_unpause_ = true;
    ResetForPause();
    std::move(time_remaining_cb).Run(kNoTimestamp);
    return;
  }

  auto loaded_ranges = engine_host_->GetBufferedRanges(role_);
  if (require_seek_after_unpause_) {
    if (loaded_ranges.empty() && segments_.empty()) {
      // There should be no loaded ranges or segments after resuming from a
      // pause, so fetch some.
      FetchManifestUpdates(base::Seconds(0), std::move(time_remaining_cb));
      return;
    }

    if (loaded_ranges.empty()) {
      // Now there are segments, but there is still no new content. Fetch and
      // parse new content, before seeking.
      ContinuePartialFetching(
          base::BindOnce(std::move(time_remaining_cb), base::Seconds(0)));
      return;
    }

    require_seek_after_unpause_ = false;
    engine_host_->RequestSeek(std::get<0>(loaded_ranges.back()));

    // When the pipeline seeks, it should re-enter this state checking loop.
    std::move(time_remaining_cb).Run(kNoTimestamp);
    return;
  }

  if (loaded_ranges.size() > 1) {
    std::move(time_remaining_cb).Run(kNoTimestamp);
    engine_host_->OnError(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  if (loaded_ranges.empty()) {
    if (segments_.empty()) {
      // This is likely to happen right after the player has started playing,
      // which means we need a full update on the manifest.
      // TODO(crbug/1266991): Use the manifest frequency polling logic here too.
      MaybeFetchManifestUpdates(base::Seconds(0), std::move(time_remaining_cb));
      return;
    }

    ContinuePartialFetching(
        base::BindOnce(std::move(time_remaining_cb), base::Seconds(0)));
    return;
  }

  auto actual_buffer_time = std::get<1>(loaded_ranges.back()) - media_time;
  auto ideal_buffer_time = GetForwardBufferSize();
  if (actual_buffer_time > ideal_buffer_time) {
    // clear content older than the current media time.
    ClearOldData(media_time);

    // Finally, in this downtime, try to fetch manifest data. This has to be
    // done periodically to get new segments, and since there is a good buffer
    // currently, its a good time to do that fetch. Manifest updates are usually
    // small, and shouldn't cause the buffer to run out.
    auto delay_time = ideal_buffer_time / 1.5;
    MaybeFetchManifestUpdates(delay_time, std::move(time_remaining_cb));
    return;
  }

  if (partial_stream_ == absl::nullopt && segments_.empty()) {
    // TODO(crbug/1266991) We've run out of segments, and will demuxer
    // underflow shortly. This implies that the rendition should be
    // reselected.
    MaybeFetchManifestUpdates(base::Seconds(0), std::move(time_remaining_cb));
    return;
  }

  ContinuePartialFetching(
      base::BindOnce(std::move(time_remaining_cb), base::Seconds(0)));
}

void HlsLiveRendition::ContinuePartialFetching(base::OnceClosure cb) {
  if (partial_stream_ != absl::nullopt) {
    FetchMoreDataFromPendingStream(std::move(cb));
    return;
  }
  if (!segments_.empty()) {
    LoadSegment(*segments_.front(), std::move(cb));
    segments_.pop();
  }
}

bool HlsLiveRendition::Seek(base::TimeDelta time) {
  return false;
}

void HlsLiveRendition::CancelPendingNetworkRequests() {
  // TODO(crbug.com/1266991): Cancel requests.
}

base::TimeDelta HlsLiveRendition::GetForwardBufferSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Try to keep a buffer of at least 5x fetch time, or 3 seconds, whichever
  // is longer. These numbers were picked based on trial and error to get a
  // smooth stream.
  if (fetch_time_.count() == 0) {
    return base::Seconds(10);
  }
  return std::max(base::Seconds(10), fetch_time_.Average() * 5);
}

void HlsLiveRendition::LoadSegment(const hls::MediaSegment& segment,
                                   base::OnceClosure cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rendition_host_->ReadFromUrl(
      segment.GetUri(), /*read_chunked=*/true, segment.GetByteRange(),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &HlsLiveRendition::OnSegmentData, weak_factory_.GetWeakPtr(),
          std::move(cb), base::TimeTicks::Now())));
}

void HlsLiveRendition::FetchMoreDataFromPendingStream(base::OnceClosure cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(partial_stream_ != absl::nullopt);
  auto partial = std::move(*partial_stream_);
  partial_stream_ = absl::nullopt;
  std::move(partial).ReadChunk(
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &HlsLiveRendition::OnSegmentData, weak_factory_.GetWeakPtr(),
          std::move(cb), base::TimeTicks::Now())));
}

void HlsLiveRendition::OnSegmentData(base::OnceClosure cb,
                                     base::TimeTicks net_req_start,
                                     HlsDataSourceStream::ReadResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    engine_host_->OnError(
        {DEMUXER_ERROR_COULD_NOT_PARSE, std::move(result).error()});
    return;
  }

  // Always ensure we are parsing the entirety of the data chunk received.
  auto sequences_seen = last_sequence_number_ - first_sequence_number_;
  auto parse_end = segment_duration_upper_limit_ * (sequences_seen + 1);
  auto source = std::move(result).value();
  if (!engine_host_->AppendAndParseData(role_, base::TimeDelta(), parse_end,
                                        &parse_offset_, source.AsRawData(),
                                        source.BytesInBuffer())) {
    engine_host_->OnError(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  auto fetch_duration = base::TimeTicks::Now() - net_req_start;
  // Adjust time based on a standard 4k download chunk.
  auto scaled = (fetch_duration * source.BytesInBuffer()) / 4096;
  fetch_time_.AddSample(scaled);

  if (source.CanReadMore()) {
    source.Flush();
    partial_stream_.emplace(std::move(source));
  }

  std::move(cb).Run();
}

void HlsLiveRendition::AppendSegments(hls::MediaPlaylist* playlist) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_download_time_ = base::TimeTicks::Now();
  for (const auto& segment : playlist->GetSegments()) {
    if (first_sequence_number_ == 0) {
      first_sequence_number_ = segment->GetMediaSequenceNumber();
    }
    if (segment->GetMediaSequenceNumber() <= last_sequence_number_) {
      continue;
    }
    last_sequence_number_ = segment->GetMediaSequenceNumber();
    segments_.push(segment);
  }
}

void HlsLiveRendition::MaybeFetchManifestUpdates(
    base::TimeDelta delay,
    ManifestDemuxer::DelayCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Section 6.3.4 of the spec states that:
  // the client MUST wait for at least the target duration before attempting
  // to reload the Playlist file again, measured from the last time the client
  // began loading the Playlist file.
  auto since_last_manifest = base::TimeTicks::Now() - last_download_time_;
  auto update_after = segment_duration_upper_limit_ * (segments_.size() + 1);
  if (since_last_manifest > update_after) {
    FetchManifestUpdates(delay, std::move(cb));
    return;
  }
  std::move(cb).Run(delay);
}

void HlsLiveRendition::FetchManifestUpdates(base::TimeDelta delay,
                                            ManifestDemuxer::DelayCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rendition_host_->ReadFromUrl(
      media_playlist_uri_, /*read_chunked=*/false, absl::nullopt,
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &HlsLiveRendition::OnManifestUpdates, weak_factory_.GetWeakPtr(),
          base::TimeTicks::Now(), delay, std::move(cb))));
}

void HlsLiveRendition::OnManifestUpdates(
    base::TimeTicks download_start_time,
    base::TimeDelta delay_time,
    ManifestDemuxer::DelayCallback cb,
    HlsDataSourceStream::ReadResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    engine_host_->OnError(
        {DEMUXER_ERROR_COULD_NOT_PARSE, std::move(result).error()});
    return;
  }

  auto source = std::move(result).value();
  if (source.CanReadMore()) {
    // TODO(crbug/1266991): Log a large manifest warning.
    std::move(source).ReadAll(base::BindPostTaskToCurrentDefault(base::BindOnce(
        &HlsLiveRendition::OnManifestUpdates, weak_factory_.GetWeakPtr(),
        download_start_time, delay_time, std::move(cb))));
    return;
  }

  auto info = hls::Playlist::IdentifyPlaylist(source.AsStringPiece());
  if (!info.has_value()) {
    engine_host_->OnError(
        {DEMUXER_ERROR_COULD_NOT_PARSE, std::move(info).error()});
    return;
  }

  auto playlist = rendition_host_->ParseMediaPlaylistFromStream(
      std::move(source), media_playlist_uri_, (*info).version);
  if (!playlist.has_value()) {
    engine_host_->OnError(
        {DEMUXER_ERROR_COULD_NOT_PARSE, std::move(playlist).error()});
    return;
  }

  auto playlist_ptr = std::move(playlist).value();
  AppendSegments(playlist_ptr.get());

  base::TimeDelta fetch_time = base::TimeTicks::Now() - download_start_time;
  std::move(cb).Run(std::max(base::Seconds(0), delay_time - fetch_time));
}

void HlsLiveRendition::ClearOldData(base::TimeDelta time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // 5 seconds chosen mostly arbitrarily to keep some prior buffer while not
  // keeping too much to cause memory issues.
  if (time <= base::Seconds(5)) {
    return;
  }
  engine_host_->Remove(role_, base::TimeDelta(), time - base::Seconds(5));
}

void HlsLiveRendition::ResetForPause() {
  segments_ = {};
  last_sequence_number_ = first_sequence_number_;
  partial_stream_ = absl::nullopt;
  auto loaded_ranges = engine_host_->GetBufferedRanges(role_);
  if (!loaded_ranges.empty()) {
    auto end_time = std::get<1>(loaded_ranges.back());
    engine_host_->Remove(role_, base::TimeDelta(), end_time);
  }
}

}  // namespace media
