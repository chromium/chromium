// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
                                   GURL media_playlist_uri)
    : engine_host_(engine_host),
      rendition_host_(rendition_host),
      segments_(std::make_unique<hls::SegmentStream>(
          std::move(playlist),
          /*seekable=*/duration.has_value())),
      role_(std::move(role)),
      duration_(duration),
      media_playlist_uri_(std::move(media_playlist_uri)),
      last_download_time_(base::TimeTicks::Now()) {}

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
    engine_host_->OnError(DEMUXER_ERROR_COULD_NOT_PARSE);
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

    // This will require a new manifest fetch. Clear the queue first, and tell
    // it what the new start timestamp should be. Then we request a seek to
    // the new start timestamp, which will take place after we fetch the
    // updated manifest.
    segments_->ResetExpectingFutureManifest(pause_duration + media_time);
    engine_host_->RequestSeek(pause_duration + media_time +
                              segments_->GetMaxDuration());
    FetchManifestUpdates(std::move(time_remaining_cb), base::Seconds(0));
    return;
  }

  auto ranges = engine_host_->GetBufferedRanges(role_);

  // Nothing loaded, nothing left, and not live. Time to stop.
  if (segments_->Exhausted() && duration_.has_value()) {
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

  // If media time comes before the last loaded range, then a seek probably
  // failed, and we should raise an error.
  if (std::get<0>(ranges.back()) > media_time) {
    PipelineStatus error = DEMUXER_ERROR_COULD_NOT_PARSE;
    engine_host_->OnError(std::move(error)
                              .WithData("timestamp", media_time)
                              .WithData("range_start", ranges.back().first)
                              .WithData("range_end", ranges.back().second));
    return;
  }

  auto end_of_buffer_ts = std::get<1>(ranges.back());

  // We need data ASAP, because the media time is outside the loaded ranges
  // due to a seek.
  if (media_time > end_of_buffer_ts) {
    TryFillingBuffers(std::move(time_remaining_cb), media_time);
    return;
  }

  // Paused content should just pretend that it will be resumed at 1x playback
  // speed, because that is by far the most common.
  auto denominator_rate = playback_rate ? playback_rate : 1.0;
  auto buffer_duration = (end_of_buffer_ts - media_time) / denominator_rate;
  auto ideal_buffer_duration = GetIdealBufferSize();

  if (buffer_duration < ideal_buffer_duration) {
    // There is a buffer, but it's not as big as we would like it to be.
    TryFillingBuffers(std::move(time_remaining_cb), media_time);
    return;
  }

  // Our buffers are in good shape. This is a good chance to clear out old
  // data and then, for live content, see if the manifest is in need of an
  // update.
  ClearOldSegments(media_time);

  // We want to delay until the buffer is ~halfway full.
  auto delay_time = buffer_duration - (ideal_buffer_duration / 2);

  if (IsLive()) {
    // Use this time to consider updating the manifest.
    MaybeFetchManifestUpdates(std::move(time_remaining_cb), delay_time);
    return;
  }

  std::move(time_remaining_cb).Run(delay_time);
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
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("media", "HLS::FetchManifestUpdates", this,
                                    "uri", media_playlist_uri_);
  rendition_host_->UpdateRenditionManifestUri(
      role_, media_playlist_uri_,
      base::BindOnce(&HlsRenditionImpl::OnManifestUpdate,
                     weak_factory_.GetWeakPtr(), std::move(cb), delay));
}

void HlsRenditionImpl::OnManifestUpdate(ManifestDemuxer::DelayCallback cb,
                                        base::TimeDelta delay,
                                        bool success) {
  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "HLS::FetchManifestUpdates", this);
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

  decryptor_ = nullptr;
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

void HlsRenditionImpl::StartWaitingForSeek() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void HlsRenditionImpl::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_stopped_for_shutdown_ = true;
}

void HlsRenditionImpl::UpdatePlaylist(
    scoped_refptr<hls::MediaPlaylist> playlist,
    std::optional<GURL> new_playlist_uri) {
  segments_->SetNewPlaylist(std::move(playlist));
  if (new_playlist_uri.has_value()) {
    media_playlist_uri_ = new_playlist_uri.value();
  }
}

base::TimeDelta HlsRenditionImpl::ClearOldSegments(base::TimeDelta media_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_stopped_for_shutdown_);
  base::TimeTicks removal_start = base::TimeTicks::Now();
  // Keep 10 seconds of content before the current media time. On mobile, a very
  // common pattern is to make double tapping the left side of the player
  // trigger a 10 second seek backwards. Keeping 10 seconds of time prior to the
  // current media time allows for a ten second rewind seek without triggering
  // new data loads. For live content, we keep 2 seconds instead, since seeking
  // is disallowed. Some content needs to be kept in the buffer to make sure
  // that bounds checking stays clean.
  base::TimeDelta remove_until;
  if (IsLive()) {
    remove_until = media_time - base::Seconds(2);
  } else {
    remove_until = media_time - kBufferDuration - segments_->GetMaxDuration();
  }

  if ((IsLive() && remove_until > base::Seconds(0)) ||
      remove_until > kBufferDuration) {
    engine_host_->Remove(role_, base::TimeDelta(), remove_until);
  }

  return base::TimeTicks::Now() - removal_start;
}

void HlsRenditionImpl::FetchNext(base::OnceClosure cb, base::TimeDelta time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_stopped_for_shutdown_);
  CHECK(!segments_->Exhausted());

  scoped_refptr<hls::MediaSegment> segment;
  base::TimeDelta segment_start;
  base::TimeDelta segment_end;
  std::tie(segment, segment_start, segment_end) = segments_->GetNextSegment();

  // If this segment has a different init segment than the segment before it,
  // we need to include the init segment before we fetch. Alternatively, if
  // we've seeked somewhere and flushed old data, we'll need the init segment
  // again.
  bool include_init = requires_init_segment_ || segment->HasNewInitSegment();

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2("media", "HLS::FetchSegment", this, "start",
                                    segment_start, "include init",
                                    include_init);

  bool is_fetching_new_key = false;
  if (auto enc = segment->GetEncryptionData()) {
    is_fetching_new_key = enc->NeedsKeyFetch();
  }
  rendition_host_->ReadMediaSegment(
      *segment, /*read_chunked=*/false, include_init,
      base::BindOnce(&HlsRenditionImpl::OnSegmentData,
                     weak_factory_.GetWeakPtr(), segment, std::move(cb), time,
                     segment_end, base::TimeTicks::Now(), is_fetching_new_key));
}

void HlsRenditionImpl::OnSegmentData(scoped_refptr<hls::MediaSegment> segment,
                                     base::OnceClosure cb,
                                     base::TimeDelta required_time,
                                     base::TimeDelta parse_end,
                                     base::TimeTicks net_req_start,
                                     bool fetched_new_key,
                                     HlsDataSourceProvider::ReadResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "HLS::FetchSegment", this);
  if (is_stopped_for_shutdown_) {
    std::move(cb).Run();
    return;
  }

  if (!result.has_value()) {
    // Drop |cb| here, and let the abort handler pick up the pieces.
    // TODO(crbug.com/40057824): If a seek abort interrupts us, we want to not
    // bubble the error upwards.
    return engine_host_->OnError(
        {DEMUXER_ERROR_COULD_NOT_PARSE, std::move(result).error()});
  }

  std::unique_ptr<HlsDataSourceStream> stream = std::move(result).value();
  DCHECK(!stream->CanReadMore());

  // This plaintext vector needs to be declared in the same scope as the
  // `AppendAndParseData` call, as it will be the memory backing for the span
  // which that function consumes. Declaring it elsewhere would lead to a
  // potential use-after-free or stack smash.
  std::vector<uint8_t> plaintext;
  base::span<const uint8_t> stream_data =
      base::span(stream->raw_data(), stream->buffer_size());

  if (auto enc_data = segment->GetEncryptionData()) {
    switch (enc_data->GetMethod()) {
      case hls::XKeyTagMethod::kAES128:
      case hls::XKeyTagMethod::kAES256: {
        if (!decryptor_ || segment->HasNewEncryptionData() || fetched_new_key) {
          // Create a new decryptor any time we get a new uri.
          decryptor_ = std::make_unique<crypto::Encryptor>();

          // Hold on to the segment - this is likely the last reference to it,
          // and it contains our aes key.
          segment_with_key_ = segment;

          auto mode = crypto::Encryptor::Mode::CBC;

          auto maybe_iv = enc_data->GetIVStr(segment->GetMediaSequenceNumber());
          if (!maybe_iv.has_value()) {
            engine_host_->OnError(DEMUXER_ERROR_COULD_NOT_PARSE);
            return;
          }
          auto iv = std::move(maybe_iv).value();
          if (!decryptor_->Init(enc_data->GetKey(), mode, iv)) {
            engine_host_->OnError(DEMUXER_ERROR_COULD_NOT_PARSE);
            return;
          }
        }

        // Decrypt the ciphertext, and re-assign the data span to point to the
        // cleartext memory in `plaintext`.
        if (!decryptor_->Decrypt(stream_data, &plaintext)) {
          return engine_host_->OnError(DEMUXER_ERROR_COULD_NOT_PARSE);
        }
        stream_data = base::span(plaintext.data(), plaintext.size());
        if (plaintext.size() == 0) {
          FetchNext(std::move(cb), required_time);
          return;
        }
        break;
      }
      default:
        break;
    }
  }

  if (last_discontinuity_sequence_num_.value_or(
          segment->GetDiscontinuitySequenceNumber()) !=
      segment->GetDiscontinuitySequenceNumber()) {
    engine_host_->ResetParserState(role_, parse_end + base::Seconds(1),
                                   &parse_offset_);
  }

  if (!engine_host_->AppendAndParseData(role_, parse_end + base::Seconds(1),
                                        &parse_offset_, stream_data)) {
    return engine_host_->OnError(DEMUXER_ERROR_COULD_NOT_PARSE);
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
  if (ranges.size() && ranges.contains(ranges.size() - 1, required_time)) {
    std::move(cb).Run();
    return;
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
