// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MP4_FRAGMENT_BOX_WRITER_H_
#define MEDIA_MUXERS_MP4_FRAGMENT_BOX_WRITER_H_

#include "media/formats/mp4/writable_box_definitions.h"
#include "media/muxers/mp4_box_writer.h"

// The file contains the box writer of `moof` and `mdat` and its children.
namespace media {

DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieFragmentBoxWriter,
                             mp4::writable_boxes::MovieFragment);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MovieFragmentHeaderBoxWriter,
                             mp4::writable_boxes::MovieFragmentHeader);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4TrackFragmentBoxWriter,
                             mp4::writable_boxes::TrackFragment);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4TrackFragmentHeaderBoxWriter,
                             mp4::writable_boxes::TrackFragmentHeader);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4TrackFragmentDecodeTimeBoxWriter,
                             mp4::writable_boxes::TrackFragmentDecodeTime);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4TrackFragmentRunBoxWriter,
                             mp4::writable_boxes::TrackFragmentRun);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4MediaDataBoxWriter,
                             mp4::writable_boxes::MediaData);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4FragmentRandomAccessBoxWriter,
                             mp4::writable_boxes::FragmentRandomAccess);
DECLARE_MP4_BOX_WRITER_CLASS(Mp4TrackFragmentRandomAccessBoxWriter,
                             mp4::writable_boxes::TrackFragmentRandomAccess);
DECLARE_MP4_BOX_WRITER_CLASS_NO_DATA(Mp4FragmentRandomAccessOffsetBoxBoxWriter);
}  // namespace media

#endif  // MEDIA_MUXERS_MP4_FRAGMENT_BOX_WRITER_H_
