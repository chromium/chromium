// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/mp4_muxer_delegate.h"

#include "media/base/audio_parameters.h"
#include "media/formats/mp4/box_definitions.h"

namespace media {

Mp4MuxerDelegate::Mp4MuxerDelegate(Muxer::WriteDataCB write_callback) {}

Mp4MuxerDelegate::~Mp4MuxerDelegate() = default;

void Mp4MuxerDelegate::AddVideoFrame(const Muxer::VideoParameters& params,
                                     base::StringPiece encoded_data,
                                     base::TimeTicks timestamp) {
  if (video_track_index_ == -1) {
    video_track_index_ = GetNextTrackIndex();
    PopulateInitialVideoTrack(params, encoded_data, video_track_index_);
  }

  PopulateVideoFragment(params, encoded_data, timestamp);
}

void Mp4MuxerDelegate::AddAudioFrame(
    const AudioParameters& params,
    base::StringPiece encoded_data,
    const AudioEncoder::CodecDescription& codec_description,
    base::TimeTicks timestamp) {
  NOTIMPLEMENTED();
}

void Mp4MuxerDelegate::Flush() {
  NOTIMPLEMENTED();
  PopulateMovieHeader();
}

void Mp4MuxerDelegate::PopulateMovieHeader() {
  NOTIMPLEMENTED();
}

void Mp4MuxerDelegate::PopulateInitialVideoTrack(
    const Muxer::VideoParameters& params,
    base::StringPiece encoded_data,
    int index) {
  NOTIMPLEMENTED();
}

void Mp4MuxerDelegate::PopulateVideoFragment(
    const Muxer::VideoParameters& params,
    base::StringPiece encoded_data,
    base::TimeTicks timestamp) {
  NOTIMPLEMENTED();
}

void Mp4MuxerDelegate::AddSampleDataToTrunAndMdat(
    mp4::writable_boxes::TrackFragmentRun& trun,
    Mp4MuxerDelegate::Fragment* fragment,
    Mp4MuxerNaluReader& nalu_reader,
    base::StringPiece encoded_data,
    base::TimeTicks timestamp,
    uint32_t timescale) {
  DCHECK_NE(video_track_index_, -1);

  AddSampleDuration(trun, timestamp, timescale);

  // The parameter sets are supplied in-band at the sync samples.
  // It is a default on encoded stream, see
  // `VideoEncoder::produce_annexb=false`.

  // Copy the data to the mdat.
  // TODO(crbug.com/1458518): We'll want to store the data as a vector of
  // encoded buffers instead of a single block so you don't have to resize
  // a giant blob of memory to hold them all. We should only have one
  // copy into the final muxed output buffer in an ideal world.
  std::vector<uint8_t>& video_data =
      fragment->mdat.track_data[video_track_index_];
  size_t current_size = video_data.size();
  if (current_size + encoded_data.size() > video_data.capacity()) {
    video_data.reserve((current_size + encoded_data.size()) * 1.5);
  }

  // TODO(crbug.com/1458518): encoded stream needs to be movable container.
  video_data.resize(current_size + encoded_data.size());
  memcpy(&video_data[current_size], encoded_data.data(), encoded_data.size());
}

void Mp4MuxerDelegate::AddSampleDuration(
    mp4::writable_boxes::TrackFragmentRun& trun,
    base::TimeTicks timestamp,
    uint32_t timescale) {
  NOTIMPLEMENTED();
}

int Mp4MuxerDelegate::GetNextTrackIndex() {
  return next_track_index_++;
}

Mp4MuxerDelegate::Fragment::Fragment() = default;
}  // namespace media.
