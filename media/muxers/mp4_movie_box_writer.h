// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MP4_MOVIE_BOX_WRITER_H_
#define MEDIA_MUXERS_MP4_MOVIE_BOX_WRITER_H_

#include "base/sequence_checker.h"
#include "media/base/media_export.h"
#include "media/formats/mp4/fourccs.h"
#include "media/formats/mp4/writable_box_definitions.h"
#include "media/muxers/mp4_box_writer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// The file contains the box writer of `moov` and its children.
namespace media {

class Mp4MuxerContext;

#define DECLARE_MP4_WRITER_CLASS_WITH_BOX(CLASS_NAME, BOX_TYPE)      \
  class MEDIA_EXPORT CLASS_NAME : public Mp4BoxWriter {              \
   public:                                                           \
    CLASS_NAME(const Mp4MuxerContext& context, const BOX_TYPE& box); \
    ~CLASS_NAME() override;                                          \
    CLASS_NAME(const CLASS_NAME&) = delete;                          \
    CLASS_NAME& operator=(const CLASS_NAME&) = delete;               \
    void Write(BoxByteStream& writer) override;                      \
                                                                     \
   private:                                                          \
    const BOX_TYPE& box_;                                            \
    SEQUENCE_CHECKER(sequence_checker_);                             \
  };

DECLARE_MP4_WRITER_CLASS_WITH_BOX(Mp4MovieBoxWriter, mp4::writable_boxes::Movie)
DECLARE_MP4_WRITER_CLASS_WITH_BOX(Mp4MovieHeaderBoxWriter,
                                  mp4::writable_boxes::MovieHeader)
DECLARE_MP4_WRITER_CLASS_WITH_BOX(Mp4MovieExtendsBoxWriter,
                                  mp4::writable_boxes::MovieExtends)
DECLARE_MP4_WRITER_CLASS_WITH_BOX(Mp4MovieTrackExtendsBoxWriter,
                                  mp4::writable_boxes::TrackExtends)

}  // namespace media

#endif  // MEDIA_MUXERS_MP4_MOVIE_BOX_WRITER_H_
