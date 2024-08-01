// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/muxers/file_webm_muxer_delegate.h"

#include <cstdint>

#include "base/containers/span.h"
#include "base/logging.h"

namespace media {

FileWebmMuxerDelegate::FileWebmMuxerDelegate(base::File webm_file)
    : webm_file_(std::move(webm_file)) {
  DCHECK(webm_file_.IsValid());
}

FileWebmMuxerDelegate::~FileWebmMuxerDelegate() = default;

void FileWebmMuxerDelegate::InitSegment(mkvmuxer::Segment* segment) {
  segment->Init(this);
  segment->set_mode(mkvmuxer::Segment::kFile);
  // According to the Matroska specs [1], it is possible to seek without the
  // Cues elements, but it would be much more difficult because a video player
  // would have to "hunt and peck through the file looking for the correct
  // timestamp". So the use of Cues are recommended, because they allow for
  // optimized seeking to absolute timestamps within the Segment.
  //
  // [1]: https://www.matroska.org/technical/cues.html.
  segment->OutputCues(true);
}

mkvmuxer::int64 FileWebmMuxerDelegate::Position() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return position_.ValueOrDie();
}

mkvmuxer::int32 FileWebmMuxerDelegate::Position(mkvmuxer::int64 position) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  position_ = position;
  webm_file_.Seek(base::File::FROM_BEGIN, position);
  return 0;
}

bool FileWebmMuxerDelegate::Seekable() const {
  return true;
}

void FileWebmMuxerDelegate::ElementStartNotify(mkvmuxer::uint64 element_id,
                                               mkvmuxer::int64 position) {}

mkvmuxer::int32 FileWebmMuxerDelegate::DoWrite(const void* buf,
                                               mkvmuxer::uint32 len) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const bool success = webm_file_.WriteAtCurrentPosAndCheck(
      base::as_bytes(base::make_span(static_cast<const uint8_t*>(buf), len)));
  LOG_IF(ERROR, !success) << "Failed to write muxer data to file.";

  return success ? 0 : -1;
}

}  // namespace media
