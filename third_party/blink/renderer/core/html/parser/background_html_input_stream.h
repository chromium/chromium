/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_BACKGROUND_HTML_INPUT_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_BACKGROUND_HTML_INPUT_STREAM_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/text/segmented_string.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

typedef wtf_size_t HTMLInputCheckpoint;

class BackgroundHTMLInputStream {
  DISALLOW_NEW();

 public:
  BackgroundHTMLInputStream();

  void Append(const String&);
  void Close();

  SegmentedString& Current() { return current_; }

  // An HTMLInputCheckpoint is valid until the next call to rewindTo, at which
  // point all outstanding checkpoints are invalidated.
  HTMLInputCheckpoint CreateCheckpoint(
      wtf_size_t tokens_extracted_since_previous_checkpoint);
  void RewindTo(HTMLInputCheckpoint, const String& unparsed_input);
  void InvalidateCheckpointsBefore(HTMLInputCheckpoint);

  wtf_size_t TotalCheckpointTokenCount() const {
    return total_checkpoint_token_count_;
  }

 private:
  struct Checkpoint {
    Checkpoint(const SegmentedString& i, wtf_size_t n, wtf_size_t t)
        : input(i),
          number_of_segments_already_appended(n),
          tokens_extracted_since_previous_checkpoint(t) {}

    SegmentedString input;
    wtf_size_t number_of_segments_already_appended;
    wtf_size_t tokens_extracted_since_previous_checkpoint;

#if DCHECK_IS_ON()
    bool IsNull() const {
      return input.IsEmpty() && !number_of_segments_already_appended;
    }
#endif
    void Clear() {
      input.Clear();
      number_of_segments_already_appended = 0;
      tokens_extracted_since_previous_checkpoint = 0;
    }
  };

  SegmentedString current_;
  Vector<String> segments_;
  Vector<Checkpoint> checkpoints_;

  // Note: These indicies may === vector.size(), in which case there are no
  // valid checkpoints/segments at this time.
  wtf_size_t first_valid_checkpoint_index_;
  wtf_size_t first_valid_segment_index_;
  wtf_size_t total_checkpoint_token_count_;

  void UpdateTotalCheckpointTokenCount();

  DISALLOW_COPY_AND_ASSIGN(BackgroundHTMLInputStream);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_BACKGROUND_HTML_INPUT_STREAM_H_
