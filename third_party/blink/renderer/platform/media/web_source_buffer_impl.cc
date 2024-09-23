// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/web_source_buffer_impl.h"

#include <stdint.h>

#include <cmath>
#include <limits>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/media_tracks.h"
#include "media/base/stream_parser.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/source_buffer_parse_warnings.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_source_buffer_client.h"

namespace blink {

static WebSourceBufferClient::ParseWarning ParseWarningToBlink(
    const media::SourceBufferParseWarning warning) {
#define CHROMIUM_PARSE_WARNING_TO_BLINK_ENUM_CASE(name) \
  case media::SourceBufferParseWarning::name:           \
    return WebSourceBufferClient::ParseWarning::name

  switch (warning) {
    CHROMIUM_PARSE_WARNING_TO_BLINK_ENUM_CASE(
        kKeyframeTimeGreaterThanDependant);
    CHROMIUM_PARSE_WARNING_TO_BLINK_ENUM_CASE(kMuxedSequenceMode);
    CHROMIUM_PARSE_WARNING_TO_BLINK_ENUM_CASE(
        kGroupEndTimestampDecreaseWithinMediaSegment);
  }

  NOTREACHED_IN_MIGRATION();
  return WebSourceBufferClient::ParseWarning::kKeyframeTimeGreaterThanDependant;

#undef CHROMIUM_PARSE_WARNING_TO_BLINK_ENUM_CASE
}

static_assert(media::kInfiniteDuration == base::TimeDelta::Max());
static_assert(media::kNoTimestamp == base::TimeDelta::Min());

static base::TimeDelta DoubleToTimeDelta(double time) {
  DCHECK(!std::isnan(time));
  DCHECK_NE(time, -std::numeric_limits<double>::infinity());

  // API sometimes needs conceptual +Infinity,
  if (time == std::numeric_limits<double>::infinity()) {
    return media::kInfiniteDuration;
  }

  base::TimeDelta converted_time = base::Seconds(time);

  // Avoid saturating finite positive input to kInfiniteDuration.
  if (converted_time == media::kInfiniteDuration) {
    return base::TimeDelta::FiniteMax();
  }

  // Avoid saturating finite negative input to KNoTimestamp.
  if (converted_time == media::kNoTimestamp) {
    return base::TimeDelta::FiniteMin();
  }

  return converted_time;
}

WebSourceBufferImpl::WebSourceBufferImpl(const std::string& id,
                                         media::ChunkDemuxer* demuxer)
    : id_(id),
      demuxer_(demuxer),
      client_(nullptr),
      append_window_end_(media::kInfiniteDuration) {
  DCHECK(demuxer_);
  demuxer_->SetTracksWatcher(
      id, base::BindRepeating(&WebSourceBufferImpl::InitSegmentReceived,
                              base::Unretained(this)));
  demuxer_->SetParseWarningCallback(
      id, base::BindRepeating(&WebSourceBufferImpl::NotifyParseWarning,
                              base::Unretained(this)));
}

WebSourceBufferImpl::~WebSourceBufferImpl() = default;

void WebSourceBufferImpl::SetClient(WebSourceBufferClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

bool WebSourceBufferImpl::GetGenerateTimestampsFlag() {
  return demuxer_->GetGenerateTimestampsFlag(id_);
}

bool WebSourceBufferImpl::SetMode(WebSourceBuffer::AppendMode mode) {
  if (demuxer_->IsParsingMediaSegment(id_))
    return false;

  switch (mode) {
    case WebSourceBuffer::kAppendModeSegments:
      demuxer_->SetSequenceMode(id_, false);
      return true;
    case WebSourceBuffer::kAppendModeSequence:
      demuxer_->SetSequenceMode(id_, true);
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

WebTimeRanges WebSourceBufferImpl::Buffered() {
  media::Ranges<base::TimeDelta> ranges = demuxer_->GetBufferedRanges(id_);
  WebTimeRanges result(ranges.size());
  for (size_t i = 0; i < ranges.size(); i++) {
    result[i].start = ranges.start(i).InSecondsF();
    result[i].end = ranges.end(i).InSecondsF();
  }
  return result;
}

double WebSourceBufferImpl::HighestPresentationTimestamp() {
  return demuxer_->GetHighestPresentationTimestamp(id_).InSecondsF();
}

bool WebSourceBufferImpl::EvictCodedFrames(double currentPlaybackTime,
                                           size_t newDataSize) {
  return demuxer_->EvictCodedFrames(id_, base::Seconds(currentPlaybackTime),
                                    newDataSize);
}

bool WebSourceBufferImpl::AppendToParseBuffer(
    base::span<const unsigned char> data) {
  return demuxer_->AppendToParseBuffer(id_, data);
}

media::StreamParser::ParseStatus WebSourceBufferImpl::RunSegmentParserLoop(
    double* timestamp_offset) {
  base::TimeDelta old_offset = timestamp_offset_;
  media::StreamParser::ParseStatus parse_result =
      demuxer_->RunSegmentParserLoop(id_, append_window_start_,
                                     append_window_end_, &timestamp_offset_);

  // Coded frame processing may update the timestamp offset. If the caller
  // provides a non-nullptr |timestamp_offset| and frame processing changes the
  // timestamp offset, report the new offset to the caller. Do not update the
  // caller's offset otherwise, to preserve any pre-existing value that may have
  // more than microsecond precision.
  if (timestamp_offset && old_offset != timestamp_offset_)
    *timestamp_offset = timestamp_offset_.InSecondsF();

  return parse_result;
}

bool WebSourceBufferImpl::AppendChunks(
    std::unique_ptr<media::StreamParser::BufferQueue> buffer_queue,
    double* timestamp_offset) {
  base::TimeDelta old_offset = timestamp_offset_;
  bool success =
      demuxer_->AppendChunks(id_, std::move(buffer_queue), append_window_start_,
                             append_window_end_, &timestamp_offset_);

  // Like in ::Append, timestamp_offset may be updated by coded frame
  // processing.
  // TODO(crbug.com/1144908): Consider refactoring this common bit into helper.
  if (timestamp_offset && old_offset != timestamp_offset_)
    *timestamp_offset = timestamp_offset_.InSecondsF();

  return success;
}

void WebSourceBufferImpl::ResetParserState() {
  demuxer_->ResetParserState(id_,
                             append_window_start_, append_window_end_,
                             &timestamp_offset_);

  // TODO(wolenetz): resetParserState should be able to modify the caller
  // timestamp offset (just like WebSourceBufferImpl::append).
  // See http://crbug.com/370229 for further details.
}

void WebSourceBufferImpl::Remove(double start, double end) {
  DCHECK_GE(start, 0);
  DCHECK_GE(end, 0);

  const auto timedelta_start = DoubleToTimeDelta(start);
  const auto timedelta_end = DoubleToTimeDelta(end);

  // Since `start - end` may be less than 1 microsecond and base::TimeDelta is
  // limited to microseconds, treat smaller ranges as zero.
  //
  // We could throw an error here, but removing nanosecond ranges is allowed by
  // the spec and the risk of breaking existing sites is high.
  if (timedelta_start == timedelta_end) {
    return;
  }

  demuxer_->Remove(id_, timedelta_start, timedelta_end);
}

bool WebSourceBufferImpl::CanChangeType(const WebString& content_type,
                                        const WebString& codecs) {
  return demuxer_->CanChangeType(id_, content_type.Utf8(), codecs.Utf8());
}

void WebSourceBufferImpl::ChangeType(const WebString& content_type,
                                     const WebString& codecs) {
  // Caller must first call ResetParserState() to flush any pending frames.
  DCHECK(!demuxer_->IsParsingMediaSegment(id_));

  demuxer_->ChangeType(id_, content_type.Utf8(), codecs.Utf8());
}

bool WebSourceBufferImpl::SetTimestampOffset(double offset) {
  if (demuxer_->IsParsingMediaSegment(id_))
    return false;

  timestamp_offset_ = DoubleToTimeDelta(offset);

  // http://www.w3.org/TR/media-source/#widl-SourceBuffer-timestampOffset
  // Step 6: If the mode attribute equals "sequence", then set the group start
  // timestamp to new timestamp offset.
  demuxer_->SetGroupStartTimestampIfInSequenceMode(id_, timestamp_offset_);
  return true;
}

void WebSourceBufferImpl::SetAppendWindowStart(double start) {
  DCHECK_GE(start, 0);
  append_window_start_ = DoubleToTimeDelta(start);
}

void WebSourceBufferImpl::SetAppendWindowEnd(double end) {
  DCHECK_GE(end, 0);
  append_window_end_ = DoubleToTimeDelta(end);
}

void WebSourceBufferImpl::RemovedFromMediaSource() {
  demuxer_->RemoveId(id_);
  demuxer_ = nullptr;
  client_ = nullptr;
}

WebMediaPlayer::TrackType mediaTrackTypeToBlink(media::MediaTrack::Type type) {
  switch (type) {
    case media::MediaTrack::Type::kAudio:
      return WebMediaPlayer::kAudioTrack;
    case media::MediaTrack::Type::kVideo:
      return WebMediaPlayer::kVideoTrack;
  }
  NOTREACHED();
}

void WebSourceBufferImpl::InitSegmentReceived(
    std::unique_ptr<media::MediaTracks> tracks) {
  DCHECK(tracks.get());
  DVLOG(1) << __func__ << " tracks=" << tracks->tracks().size();

  std::vector<WebSourceBufferClient::MediaTrackInfo> trackInfoVector;
  for (const auto& track : tracks->tracks()) {
    WebSourceBufferClient::MediaTrackInfo trackInfo;
    trackInfo.track_type = mediaTrackTypeToBlink(track->type());
    trackInfo.id = WebString::FromUTF8(track->track_id().value());
    trackInfo.byte_stream_track_id =
        WebString::FromUTF8(base::NumberToString(track->stream_id()));
    trackInfo.kind = WebString::FromUTF8(track->kind().value());
    trackInfo.label = WebString::FromUTF8(track->label().value());
    trackInfo.language = WebString::FromUTF8(track->language().value());
    trackInfoVector.push_back(trackInfo);
  }

  client_->InitializationSegmentReceived(trackInfoVector);
}

void WebSourceBufferImpl::NotifyParseWarning(
    const media::SourceBufferParseWarning warning) {
  client_->NotifyParseWarning(ParseWarningToBlink(warning));
}

}  // namespace blink
