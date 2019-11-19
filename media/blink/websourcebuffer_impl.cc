// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/websourcebuffer_impl.h"

#include <stdint.h>

#include <cmath>
#include <limits>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/media_tracks.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/chunk_demuxer.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_source_buffer_client.h"

namespace media {

static blink::WebSourceBufferClient::ParseWarning ParseWarningToBlink(
    const SourceBufferParseWarning warning) {
#define CHROMIUM_PARSE_WARNING_TO_BLINK_ENUM_CASE(name) \
  case SourceBufferParseWarning::name:                  \
    return blink::WebSourceBufferClient::ParseWarning::name

  switch (warning) {
    CHROMIUM_PARSE_WARNING_TO_BLINK_ENUM_CASE(
        kKeyframeTimeGreaterThanDependant);
    CHROMIUM_PARSE_WARNING_TO_BLINK_ENUM_CASE(kMuxedSequenceMode);
    CHROMIUM_PARSE_WARNING_TO_BLINK_ENUM_CASE(
        kGroupEndTimestampDecreaseWithinMediaSegment);
  }

  NOTREACHED();
  return blink::WebSourceBufferClient::ParseWarning::
      kKeyframeTimeGreaterThanDependant;

#undef CHROMIUM_PARSE_WARNING_TO_BLINK_ENUM_CASE
}

static base::TimeDelta DoubleToTimeDelta(double time) {
  DCHECK(!std::isnan(time));
  DCHECK_NE(time, -std::numeric_limits<double>::infinity());

  if (time == std::numeric_limits<double>::infinity())
    return kInfiniteDuration;

  // Don't use base::TimeDelta::Max() here, as we want the largest finite time
  // delta.
  base::TimeDelta max_time = base::TimeDelta::FromInternalValue(
      std::numeric_limits<int64_t>::max() - 1);
  double max_time_in_seconds = max_time.InSecondsF();

  if (time >= max_time_in_seconds)
    return max_time;

  return base::TimeDelta::FromMicroseconds(
      time * base::Time::kMicrosecondsPerSecond);
}

WebSourceBufferImpl::WebSourceBufferImpl(const std::string& id,
                                         ChunkDemuxer* demuxer)
    : id_(id),
      demuxer_(demuxer),
      client_(NULL),
      append_window_end_(kInfiniteDuration) {
  DCHECK(demuxer_);
  demuxer_->SetTracksWatcher(
      id, base::Bind(&WebSourceBufferImpl::InitSegmentReceived,
                     base::Unretained(this)));
  demuxer_->SetParseWarningCallback(
      id, base::Bind(&WebSourceBufferImpl::NotifyParseWarning,
                     base::Unretained(this)));
}

WebSourceBufferImpl::~WebSourceBufferImpl() = default;

void WebSourceBufferImpl::SetClient(blink::WebSourceBufferClient* client) {
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

  NOTREACHED();
  return false;
}

blink::WebTimeRanges WebSourceBufferImpl::Buffered() {
  Ranges<base::TimeDelta> ranges = demuxer_->GetBufferedRanges(id_);
  blink::WebTimeRanges result(ranges.size());
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
  return demuxer_->EvictCodedFrames(
      id_,
      base::TimeDelta::FromSecondsD(currentPlaybackTime),
      newDataSize);
}

bool WebSourceBufferImpl::Append(const unsigned char* data,
                                 unsigned length,
                                 double* timestamp_offset) {
  base::TimeDelta old_offset = timestamp_offset_;
  bool success = demuxer_->AppendData(id_, data, length, append_window_start_,
                                      append_window_end_, &timestamp_offset_);

  // Coded frame processing may update the timestamp offset. If the caller
  // provides a non-NULL |timestamp_offset| and frame processing changes the
  // timestamp offset, report the new offset to the caller. Do not update the
  // caller's offset otherwise, to preserve any pre-existing value that may have
  // more than microsecond precision.
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
  demuxer_->Remove(id_, DoubleToTimeDelta(start), DoubleToTimeDelta(end));
}

bool WebSourceBufferImpl::CanChangeType(const blink::WebString& content_type,
                                        const blink::WebString& codecs) {
  return demuxer_->CanChangeType(id_, content_type.Utf8(), codecs.Utf8());
}

void WebSourceBufferImpl::ChangeType(const blink::WebString& content_type,
                                     const blink::WebString& codecs) {
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
  demuxer_ = NULL;
  client_ = NULL;
}

blink::WebMediaPlayer::TrackType mediaTrackTypeToBlink(MediaTrack::Type type) {
  switch (type) {
    case MediaTrack::Audio:
      return blink::WebMediaPlayer::kAudioTrack;
    case MediaTrack::Text:
      return blink::WebMediaPlayer::kTextTrack;
    case MediaTrack::Video:
      return blink::WebMediaPlayer::kVideoTrack;
  }
  NOTREACHED();
  return blink::WebMediaPlayer::kAudioTrack;
}

void WebSourceBufferImpl::InitSegmentReceived(
    std::unique_ptr<MediaTracks> tracks) {
  DCHECK(tracks.get());
  DVLOG(1) << __func__ << " tracks=" << tracks->tracks().size();

  std::vector<blink::WebSourceBufferClient::MediaTrackInfo> trackInfoVector;
  for (const auto& track : tracks->tracks()) {
    blink::WebSourceBufferClient::MediaTrackInfo trackInfo;
    trackInfo.track_type = mediaTrackTypeToBlink(track->type());
    trackInfo.id = blink::WebString::FromUTF8(track->id().value());
    trackInfo.byte_stream_track_id = blink::WebString::FromUTF8(
        base::NumberToString(track->bytestream_track_id()));
    trackInfo.kind = blink::WebString::FromUTF8(track->kind().value());
    trackInfo.label = blink::WebString::FromUTF8(track->label().value());
    trackInfo.language = blink::WebString::FromUTF8(track->language().value());
    trackInfoVector.push_back(trackInfo);
  }

  client_->InitializationSegmentReceived(trackInfoVector);
}

void WebSourceBufferImpl::NotifyParseWarning(
    const SourceBufferParseWarning warning) {
  client_->NotifyParseWarning(ParseWarningToBlink(warning));
}

}  // namespace media
