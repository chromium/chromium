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

class MEDIA_EXPORT Mp4MovieBoxWriter : public Mp4BoxWriter {
 public:
  Mp4MovieBoxWriter(const Mp4MuxerContext& context,
                    const mp4::writable_boxes::Movie& box);
  ~Mp4MovieBoxWriter() override;

  Mp4MovieBoxWriter(const Mp4MovieBoxWriter&) = delete;
  Mp4MovieBoxWriter& operator=(const Mp4MovieBoxWriter&) = delete;

  void Write(BoxByteStream& writer) override;

 private:
  const mp4::writable_boxes::Movie& movie_box_;
  SEQUENCE_CHECKER(sequence_checker_);
};

class MEDIA_EXPORT Mp4MovieHeaderBoxWriter : public Mp4BoxWriter {
 public:
  Mp4MovieHeaderBoxWriter(const Mp4MuxerContext& context,
                          const mp4::writable_boxes::MovieHeader& box);
  ~Mp4MovieHeaderBoxWriter() override;

  Mp4MovieHeaderBoxWriter(const Mp4MovieHeaderBoxWriter&) = delete;
  Mp4MovieHeaderBoxWriter& operator=(const Mp4MovieHeaderBoxWriter&) = delete;

  void Write(BoxByteStream& writer) override;

 private:
  const mp4::writable_boxes::MovieHeader& movie_header_box_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MUXERS_MP4_MOVIE_BOX_WRITER_H_
