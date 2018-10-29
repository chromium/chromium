/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/markers/document_marker.h"

#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

DocumentMarker::~DocumentMarker() = default;

DocumentMarker::DocumentMarker(unsigned start_offset, unsigned end_offset)
    : start_offset_(start_offset), end_offset_(end_offset) {
  DCHECK_LT(start_offset_, end_offset_);
}

base::Optional<DocumentMarker::MarkerOffsets>
DocumentMarker::ComputeOffsetsAfterShift(unsigned offset,
                                         unsigned old_length,
                                         unsigned new_length) const {
  MarkerOffsets result;
  result.start_offset = StartOffset();
  result.end_offset = EndOffset();

  // algorithm inspired by https://dom.spec.whatwg.org/#concept-cd-replace
  // but with some changes

  // Deviation from the concept-cd-replace algorithm: second condition in the
  // next line (don't include text inserted immediately before a marker in the
  // marked range, but do include the new text if it's replacing text in the
  // marked range)
  if (StartOffset() > offset || (StartOffset() == offset && old_length == 0)) {
    if (StartOffset() <= offset + old_length) {
      // Marker start was in the replaced text. Move to end of new text
      // (Deviation from the concept-cd-replace algorithm: that algorithm
      // would move to the beginning of the new text here)
      result.start_offset = offset + new_length;
    } else {
      // Marker start was after the replaced text. Shift by length
      // difference
      result.start_offset = StartOffset() + new_length - old_length;
    }
  }

  if (EndOffset() > offset) {
    // Deviation from the concept-cd-replace algorithm: < instead of <= in
    // the next line
    if (EndOffset() < offset + old_length) {
      // Marker end was in the replaced text. Move to beginning of new text
      result.end_offset = offset;
    } else {
      // Marker end was after the replaced text. Shift by length difference
      result.end_offset = EndOffset() + new_length - old_length;
    }
  }

  if (result.start_offset >= result.end_offset)
    return base::nullopt;

  return result;
}

void DocumentMarker::ShiftOffsets(int delta) {
  start_offset_ += delta;
  end_offset_ += delta;
}

}  // namespace blink
