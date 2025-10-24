// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media/filters/hls_rendition_impl.h"

#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "media/filters/hls_manifest_demuxer_engine.h"

namespace media {

constexpr base::TimeDelta kBufferDuration = base::Seconds(10);

HlsRenditionImpl::~HlsRenditionImpl() {
  engine_host_->RemoveRole(role_);
}

HlsRenditionImpl::HlsRenditionImpl(ManifestDemuxerEngineHost* engine_host,
                                   HlsRenditionHost* rendition_host,
                                   std::string role,
                                   scoped_refptr<hls::MediaPlaylist> playlist,
                                   std::optional<base::TimeDelta> duration,
                                   GURL media_playlist_uri,
                                   MediaLog* media_log)
    : engine_host_(engine_host),
      rendition_host_(rendition_host),
      segments_(std::make_unique<hls::SegmentStream>(
          std::move(playlist),
          /*seekable=*/duration.has_value())),
      role_(std::move(role)),
      duration_(duration),
      media_playlist_uri_(std::move(media_playlist_uri)),
      last_download_time_(base::TimeTicks::Now()),
      media_log_(media_log->Clone()) {}

std::optional<base::TimeDelta> HlsRenditionImpl::GetDuration() {
  return duration_;
}

void HlsRenditionImpl::CheckState(
    base::TimeDelta media_time,
    double playback_rate,
    ManifestDemuxer::DelayCallback time_remaining_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_stopped_for_shutdown_ || !segments_->PlaylistHasSegments()) {
    std::move(time_remaining_cb).Run(kNoTimestamp);
    return;
  }

  if (IsLive() && playback_rate != 1 && playback_rate != 0) {
    // TODO(crbug.com/40057824): What should be done about non-paused,
    // non-real-time playback? Anything above 1 would hit the end and constantly
    // be in a state of demuxer underflow, and anything slower than 1 would
    // eventually have so much data buffered that it would OOM.
    rendition_host_->Quit(HlsDemuxerStatus::Codes::kInvalidLivePlaybackRate);
    return;
  }

  if (playback_rate != 0.0) {
    has_ever_played_ = true;
  }

  if (IsLive() && playback_rate == 0.0 && !livestream_pause_time_) {
    // Any time there is a paused state check and the initial pause timestamp
    // isn't set, we have to set it.
    livestream_pause_time_ = base::TimeTicks::Now();
  }

  if (IsLive() && playback_rate == 0.0 && has_ever_played_) {
    std::move(time_remaining_cb).Run(kNoTimestamp);
    return;
  }

  if (playback_rate == 1.0 && livestream_pause_time_.has_value()) {
    CHECK_EQ(playback_rate, 1.0);

    auto remaining_manifest_time =
        segments_->QueueSize() * segments_->GetMaxDuration();
    auto pause_duration = base::TimeTicks::Now() - *livestream_pause_time_;
    livestream_pause_time_ = std::nullopt;

    if (pause_duration < remaining_manifest_time) {
      // our pause was so short that we are still within the segments currently
      // available since our last fetch. All we have to do is seek to the
      // correct time. At the moment, `media_time` will still be the old time
      // when the pause happened. Seeking will restart the CheckState loop.
      engine_host_->RequestSeek(pause_duration + media_time);
      std::move(time_remaining_cb).Run(kNoTimestamp);
      return;
    }

    ResumeLivePlayback(
        pause_duration + media_time + segments_->GetMaxDuration(),
        base::BindOnce(std::move(time_remaining_cb), kNoTimestamp));
    return;
  }

  auto ranges = engine_host_->GetBufferedRanges(role_);

  // Nothing loaded, nothing left, and not live. Time to stop.
  if (segments_->Exhausted() && duration_.has_value()) {
    if (!has_ever_played_ && ranges.empty()) {
      // If we've never loaded any content, and never played anything, then
      // we can only get into this state if the media content is super messed
      // up and chunk demuxer loads a bunch of it without ever initializing.
      // This is an error.
      rendition_host_->Quit(HlsDemuxerStatus::Codes::kNoDataEverAppended);
      return;
    }
    if (!set_stream_end_) {
      set_stream_end_ = true;
      rendition_host_->SetEndOfStream(true);
    }
    std::move(time_remaining_cb).Run(kNoTimestamp);
    return;
  }

  // Consider re-requesting.
  if (ranges.empty() && segments_->Exhausted()) {
    MaybeFetchManifestUpdates(std::move(time_remaining_cb), base::Seconds(0));
    return;
  }

  // Fetch the next segment.
  if (ranges.empty()) {
    FetchNext(base::BindOnce(std::move(time_remaining_cb), base::Seconds(0)),
              media_time);
    return;
  }

  if (ranges.contains(0, media_time)) {
    auto end_of_range_ts = ranges.end(0);
    auto end_of_buffer_ts = std::get<1>(ranges.back());
    auto ideal_buffer_duration = GetIdealBufferSize();
    auto denominator_rate = playback_rate ? playback_rate : 1.0;
    auto range_duration = (end_of_range_ts - media_time) / denominator_rate;
    auto buffer_duration = (end_of_buffer_ts - media_time) / denominator_rate;

    // Regardless of the gaps ahead, we just don't have enough data at all, and
    // we need more.
    if (buffer_duration < ideal_buffer_duration) {
      TryFillingBuffers(std::move(time_remaining_cb), media_time);
      return;
    }

    // If there are gaps in the loaded content, we might have to issue a seek
    // really soon to skip them. We want to do this after `media_time` has
    // run into that gap in order to calculate the best seek offset, so we issue
    // a delay here for as much time as is needed to hit the end of this
    // particular range.
    if (range_duration < ideal_buffer_duration) {
      // wait to check state again when `media_time` is at the gap coming up.
      std::move(time_remaining_cb).Run(range_duration);
      return;
    }

    // Why do today what can be put off until tomorrow? Handle the gap later.
    // Our buffers are in good shape. This is a good chance to clear out old
    // data and then, for live content, see if the manifest is in need of an
    // update. We then want to delay until the buffer is ~halfway full.
    ClearOldSegments(media_time);
    auto delay_time = range_duration - (ideal_buffer_duration / 2);

    if (IsLive()) {
      MaybeFetchManifestUpdates(std::move(time_remaining_cb), delay_time);
      return;
    }
    std::move(time_remaining_cb).Run(delay_time);
    return;
  }

  constexpr base::TimeDelta kMinimumSpottyBufferSeek = base::Milliseconds(17);
  constexpr base::TimeDelta kMaximumGapSkipDistance = base::Seconds(2);
  // When `media_time` is after the first buffer, it's because of a gap in
  // playback. The criteria for finding a seek spot are as follows:
  // 1) it must be within `kMaximumGapSkipDistance` of the end of buffer #1.
  // 2) it must be in a range having a duration >= kMinimumSpottyBufferSeek
  // 3) it must be the first range meeting these criteria
  if (media_time > ranges.end(0)) {
    for (size_t i = 1; i < ranges.size(); i++) {
      auto distance = ranges.start(i) - ranges.end(0);
      if (distance > kMaximumGapSkipDistance) {
        HlsDemuxerStatus error = {
            HlsDemuxerStatus::Codes::kInvalidLoadedRanges,
            "Unable to seek past gap in buffered ranges - gap too large"};
        rendition_host_->Quit(std::move(error)
                                  .WithData("media_time", media_time)
                                  .WithData("ranges", ranges));
        return;
      }

      auto duration = ranges.end(i) - ranges.start(i);
      if (duration > kMinimumSpottyBufferSeek) {
        // Respond with a notice to not keep checking state. We will clear data
        // and request a seek to the start of the new range found.
        engine_host_->Remove(role_, base::TimeDelta(), ranges.end(i - 1));
        ranges = engine_host_->GetBufferedRanges(role_);
        if (ranges.empty()) {
          rendition_host_->Quit({HlsDemuxerStatus::Codes::kInvalidLoadedRanges,
                                 "Unloading disjoint buffered ranges failed"});
          return;
        }

        engine_host_->RequestSeek(ranges.start(0));
        std::move(time_remaining_cb).Run(kNoTimestamp);
        return;
      }
    }

    // There was no loaded range that starts more than `kMaximumGapSkipDistance`
    // into the future, which means we're simultaneously in a state where there
    // is a gap but also where we desperately need more data. If we fill the
    // buffers more, the engine will re-check state immediately and we can
    // hopefully find a buffer to seek into.
    TryFillingBuffers(std::move(time_remaining_cb), media_time);
    return;
  }

  // If media time is before the first range, we might be able to seek to it
  // if it's close enough.
  if (ranges.start(0) - media_time < kMaximumGapSkipDistance) {
    engine_host_->RequestSeek(ranges.start(0));
    std::move(time_remaining_cb).Run(kNoTimestamp);
    return;
  }

  // The only loaded range is far into the future, which is a weird place to
  // be, and we don't have any way to recover from it.
  HlsDemuxerStatus error = HlsDemuxerStatus::Codes::kInvalidLoadedRanges;
  rendition_host_->Quit(std::move(error)
                            .WithData("media_time", media_time)
                            .WithData("ranges", ranges));
}

void HlsRenditionImpl::TryFillingBuffers(ManifestDemuxer::DelayCallback delay,
                                         base::TimeDelta media_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Live content should fetch an update if the segment queue is exhausted.
  if (IsLive() && segments_->Exhausted()) {
    FetchManifestUpdates(std::move(delay), base::Seconds(0));
    return;
  }

  // VOD content has nothing to do if the segment queue is exhausted.
  if (segments_->Exhausted()) {
    std::move(delay).Run(kNoTimestamp);
    return;
  }

  // If there are segments in the queue, fetch.
  FetchNext(base::BindOnce(std::move(delay), base::Seconds(0)), media_time);
}

void HlsRenditionImpl::FetchManifestUpdates(ManifestDemuxer::DelayCallback cb,
                                            base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_stopped_for_shutdown_);
  last_download_time_ = base::TimeTicks::Now();
  TRACE_EVENT_BEGIN("media", "HLS::FetchManifestUpdates",
                    perfetto::Track::FromPointer(this), "uri",
                    media_playlist_uri_);
  rendition_host_->UpdateRenditionManifestUri(
      role_, media_playlist_uri_,
      base::BindOnce(&HlsRenditionImpl::OnManifestUpdate,
                     weak_factory_.GetWeakPtr(), std::move(cb), delay));
}

void HlsRenditionImpl::OnManifestUpdate(ManifestDemuxer::DelayCallback cb,
                                        base::TimeDelta delay,
                                        HlsDemuxerStatus success) {
  TRACE_EVENT_END("media", perfetto::Track::FromPointer(this));
  auto update_duration = base::TimeTicks::Now() - last_download_time_;
  if (update_duration > delay) {
    std::move(cb).Run(base::Seconds(0));
    return;
  }
  std::move(cb).Run(delay - update_duration);
}

void HlsRenditionImpl::MaybeFetchManifestUpdates(
    ManifestDemuxer::DelayCallback cb,
    base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_stopped_for_shutdown_);
  // Section 6.3.4 of the spec states that:
  // the client MUST wait for at least the target duration before attempting
  // to reload the Playlist file again, measured from the last time the client
  // began loading the Playlist file.
  auto since_last_manifest = base::TimeTicks::Now() - last_download_time_;
  auto update_after = segments_->QueueSize() * segments_->GetMaxDuration();
  if (since_last_manifest > update_after) {
    FetchManifestUpdates(std::move(cb), delay);
    return;
  }
  std::move(cb).Run(delay);
}

base::TimeDelta HlsRenditionImpl::GetIdealBufferSize() const {
  // TODO(crbug.com/40057824): This buffer size _could_ be based on network
  // speed and video bitrate, but it's actually quite effective to keep it just
  // at a fixed size, due to the fact that the stream adaptation will always try
  // to match an appropriate bitrate. 10 seconds was chosen based on a good
  // amount of buffer to prevent demuxer underflow when toggling between
  // wired data and fast 3g speeds.
  return kBufferDuration;
}

ManifestDemuxer::SeekResponse HlsRenditionImpl::Seek(
    base::TimeDelta seek_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_stopped_for_shutdown_) {
    return PIPELINE_ERROR_ABORT;
  }

  if (set_stream_end_) {
    set_stream_end_ = false;
    rendition_host_->SetEndOfStream(false);
  }

  auto ranges = engine_host_->GetBufferedRanges(role_);
  if (!ranges.empty() && ranges.contains(ranges.size() - 1, seek_time)) {
    // If the seek time is in the last loaded range, then there is no need to
    // update the pending fetch state or clear/flush any buffers.
    return ManifestDemuxer::SeekState::kIsReady;
  }

  key_.clear();
  last_discontinuity_sequence_num_ = std::nullopt;

  if (IsLive()) {
    return ManifestDemuxer::SeekState::kNeedsData;
  }

  // If we seek anywhere else, we should evict everything in order to avoid
  // fragmented loaded sections and large memory consumption.
  engine_host_->EvictCodedFrames(role_, base::Seconds(0), 0);
  engine_host_->RemoveAndReset(role_, base::TimeDelta(), duration_.value(),
                               &parse_offset_);

  if (segments_->Seek(seek_time)) {
    // The seek successfully put segments into the queue, se reset the sequence
    // modes.
    engine_host_->SetGroupStartIfParsingAndSequenceMode(
        role_, segments_->NextSegmentStartTime());
  }

  // If this stream uses initialization segments, we're going to need once now,
  // since chunk demuxer is empty.
  requires_init_segment_ = true;

  return ManifestDemuxer::SeekState::kNeedsData;
}

void HlsRenditionImpl::ResumeLivePlayback(base::TimeDelta estimated_resume,
                                          base::OnceClosure done) {
  // The estimated resume might not be totally correct, because live content
  // doesn't always sync up with real-time (drops and speedups). So we want to
  // clear the old manifest, grab a new one, fetch the first segment, and then
  // seek to the start of the loaded ranges that we have.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  segments_->ResetExpectingFutureManifest(estimated_resume);
  engine_host_->Remove(role_, base::TimeDelta(), estimated_resume);
  FetchManifestUpdates(
      base::BindOnce(&HlsRenditionImpl::ManifestUpdateForLiveResume,
                     weak_factory_.GetWeakPtr(), std::move(done)),
      base::Seconds(0));
}

void HlsRenditionImpl::ManifestUpdateForLiveResume(base::OnceClosure done,
                                                   base::TimeDelta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (segments_->Exhausted()) {
    rendition_host_->Quit(HlsDemuxerStatus::Codes::kNoDataEverAppended);
    std::move(done).Run();
    return;
  }
  FetchNext(base::BindOnce(&HlsRenditionImpl::FirstSegmentFetchedForLiveResume,
                           weak_factory_.GetWeakPtr(), std::move(done)),
            std::nullopt);
}

void HlsRenditionImpl::FirstSegmentFetchedForLiveResume(
    base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto ranges = engine_host_->GetBufferedRanges(role_);
  if (ranges.size() == 0) {
    rendition_host_->Quit(HlsDemuxerStatus::Codes::kNoDataEverAppended);
    std::move(done).Run();
    return;
  }
  engine_host_->RequestSeek(ranges.start(0));
  std::move(done).Run();
}

void HlsRenditionImpl::StartWaitingForSeek() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void HlsRenditionImpl::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_stopped_for_shutdown_ = true;
}

void HlsRenditionImpl::UpdatePlaylist(
    scoped_refptr<hls::MediaPlaylist> playlist) {
  segments_->SetNewPlaylist(std::move(playlist));
}

void HlsRenditionImpl::UpdatePlaylistURI(const GURL& playlist_uri) {
  media_playlist_uri_ = playlist_uri;
}

const GURL& HlsRenditionImpl::MediaPlaylistUri() const {
  return media_playlist_uri_;
}

base::TimeDelta HlsRenditionImpl::ClearOldSegments(base::TimeDelta media_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_stopped_for_shutdown_);
  base::TimeTicks removal_start = base::TimeTicks::Now();
  // A common pattern on the mobile player is a 10 second rewind triggered by a
  // double tap on the left side of the player, so we want to keep at least 10
  // seconds of older content so a seek won't require a re-fetch. We also need
  // to keep at least enough data for there to be some keyframes, which we can
  // get from the segment max duration.
  base::TimeDelta required_buffer =
      std::max(kBufferDuration, segments_->GetMaxDuration() * 2);
  if (media_time > required_buffer) {
    engine_host_->Remove(role_, base::TimeDelta(),
                         media_time - required_buffer);
  }

  auto ranges = engine_host_->GetBufferedRanges(role_);
  media_log_->SetProperty<MediaLogProperty::kHlsBufferedRanges>(ranges);

  return base::TimeTicks::Now() - removal_start;
}

void HlsRenditionImpl::FetchNext(base::OnceClosure cb,
                                 std::optional<base::TimeDelta> time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_stopped_for_shutdown_);
  CHECK(!segments_->Exhausted());

  scoped_refptr<hls::MediaSegment> segment;
  base::TimeDelta segment_start;
  base::TimeDelta segment_end;
  bool needs_init = false;
  std::tie(segment, segment_start, segment_end, needs_init) =
      segments_->GetNextSegment();

  if (segment->IsGap()) {
    TryFillingBuffers(
        base::BindOnce(
            [](base::OnceClosure cb, base::TimeDelta) { std::move(cb).Run(); },
            std::move(cb)),
        time.value_or(base::Seconds(0)));
    return;
  }

  // If this segment has a different init segment than the segment before it,
  // we need to include the init segment before we fetch. Alternatively, if
  // we've seeked somewhere and flushed old data, we'll need the init segment
  // again.
  bool include_init = requires_init_segment_ || needs_init;

  TRACE_EVENT_BEGIN("media", "HLS::FetchSegment",
                    perfetto::Track::FromPointer(this), "start", segment_start,
                    "include init", include_init);

  bool is_fetching_new_key = false;
  if (auto enc = segment->GetEncryptionData()) {
    is_fetching_new_key = enc->NeedsKeyFetch();
  }
  media_log_->AddEvent<MediaLogEvent::kHlsSegmentFetch>(
      segment->GetUri().PathForRequest());
  rendition_host_->ReadMediaSegment(
      *segment, /*read_chunked=*/false, include_init,
      base::BindOnce(&HlsRenditionImpl::OnSegmentData,
                     weak_factory_.GetWeakPtr(), segment, std::move(cb), time,
                     segment_end, base::TimeTicks::Now(), is_fetching_new_key));
}

void HlsRenditionImpl::OnSegmentData(
    scoped_refptr<hls::MediaSegment> segment,
    base::OnceClosure cb,
    std::optional<base::TimeDelta> required_time,
    base::TimeDelta parse_end,
    base::TimeTicks net_req_start,
    bool fetched_new_key,
    HlsDataSourceProvider::ReadResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_END("media", perfetto::Track::FromPointer(this));
  if (is_stopped_for_shutdown_) {
    std::move(cb).Run();
    return;
  }

  if (!result.has_value()) {
    // Drop |cb| here, and let the abort handler pick up the pieces.
    // TODO(crbug.com/40057824): If a seek abort interrupts us, we want to not
    // bubble the error upwards.
    rendition_host_->Quit(HlsDemuxerStatusTraits::FromReadStatus(
        std::move(result).error().AddHere()));
    return;
  }

  std::unique_ptr<HlsDataSourceStream> stream = std::move(result).value();
  DCHECK(!stream->CanReadMore());

  std::vector<uint8_t> mem;
  base::span<const uint8_t> stream_data;
  if (!segment->GetPlaintextStreamSource(stream->data(), &stream_data, &mem)) {
    rendition_host_->Quit(HlsDemuxerStatus::Codes::kFailedToDecryptSegment);
    return;
  }
  if (!stream_data.size() && stream->data().size()) {
    FetchNext(std::move(cb), required_time);
    return;
  }

  if (last_discontinuity_sequence_num_.value_or(
          segment->GetDiscontinuitySequenceNumber()) !=
      segment->GetDiscontinuitySequenceNumber()) {
    engine_host_->ResetParserState(role_, kInfiniteDuration, &parse_offset_);
  }

  if (!engine_host_->AppendAndParseData(role_, kInfiniteDuration,
                                        &parse_offset_, stream_data)) {
    rendition_host_->Quit(HlsDemuxerStatus::Codes::kCouldNotAppendData);
    return;
  }

  last_discontinuity_sequence_num_ = segment->GetDiscontinuitySequenceNumber();

  // Wince we've successfully parsed our data, we can mark that an init segment
  // is not required due to seeking.
  requires_init_segment_ = false;

  auto fetch_duration = base::TimeTicks::Now() - net_req_start;
  auto bps = stream->buffer_size() * 8 / fetch_duration.InSecondsF();
  rendition_host_->UpdateNetworkSpeed(bps);

  // After a seek especially, we will start loading content that comes
  // potentially much earlier than the seek time, and it's possible that the
  // loaded ranges won't yet contain the timestamp that is required to be loaded
  // for the seek to complete. In this case, we just keep fetching until
  // the seek time is loaded.
  auto ranges = engine_host_->GetBufferedRanges(role_);
  media_log_->SetProperty<MediaLogProperty::kHlsBufferedRanges>(ranges);

  if (ranges.size()) {
    if (!required_time.has_value() ||
        (*required_time >= ranges.start(0) &&
         *required_time <= std::get<1>(ranges.back()))) {
      std::move(cb).Run();
      return;
    }
  }

  // If the last range doesn't contain the timestamp, keep parsing until it
  // does. If there is nothing left to download, then we can return.
  if (segments_->Exhausted()) {
    std::move(cb).Run();
    return;
  }

  FetchNext(std::move(cb), required_time);
}

bool HlsRenditionImpl::IsLive() const {
  return !duration_.has_value();
}

}  // namespace media
