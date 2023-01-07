/*
 * Copyright (C) 2013, Opera Software ASA. All rights reserved.
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

#include "third_party/blink/renderer/core/html/track/vtt/buffered_line_reader.h"

#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

bool BufferedLineReader::GetLine(String& line) {
  if (maybe_skip_lf_) {
    // We ran out of data after a CR (U+000D), which means that we may be
    // in the middle of a CRLF pair. If the next character is a LF (U+000A)
    // then skip it, and then (unconditionally) return the buffered line.
    if (!buffer_.IsEmpty()) {
      ScanCharacter(kNewlineCharacter);
      maybe_skip_lf_ = false;
    }
    // If there was no (new) data available, then keep maybe_skip_lf_ set,
    // and fall through all the way down to the EOS check at the end of
    // the method.
  }

  bool should_return_line = false;
  bool check_for_lf = false;
  while (!buffer_.IsEmpty()) {
    UChar c = buffer_.CurrentChar();
    buffer_.Advance();

    if (c == kNewlineCharacter || c == kCarriageReturnCharacter) {
      // We found a line ending. Return the accumulated line.
      should_return_line = true;
      check_for_lf = (c == kCarriageReturnCharacter);
      break;
    }

    // NULs are transformed into U+FFFD (REPLACEMENT CHAR.) in step 1 of
    // the WebVTT parser algorithm.
    if (c == '\0')
      c = kReplacementCharacter;

    line_buffer_.Append(c);
  }

  if (check_for_lf) {
    // May be in the middle of a CRLF pair.
    if (!buffer_.IsEmpty()) {
      // Scan a potential newline character.
      ScanCharacter(kNewlineCharacter);
    } else {
      // Check for the LF on the next call (unless we reached EOS, in
      // which case we'll return the contents of the line buffer, and
      // reset state for the next line.)
      maybe_skip_lf_ = true;
    }
  }

  if (IsAtEndOfStream()) {
    // We've reached the end of the stream proper. Emit a line if the
    // current line buffer is non-empty. (Note that if shouldReturnLine is
    // set already, we want to return a line nonetheless.)
    should_return_line |= !line_buffer_.empty();
  }

  if (should_return_line) {
    line = line_buffer_.ToString();
    line_buffer_.Clear();
    return true;
  }

  DCHECK(buffer_.IsEmpty());
  return false;
}

}  // namespace blink
