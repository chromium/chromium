/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/html/track/vtt/vtt_tokenizer.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/html/parser/html_entity_parser.h"
#include "third_party/blink/renderer/core/html/parser/markup_tokenizer_inlines.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

#define WEBVTT_BEGIN_STATE(state_name) \
  case state_name:                     \
  state_name:
#define WEBVTT_ADVANCE_TO(state_name)               \
  do {                                              \
    state = state_name;                             \
    DCHECK(!input_.IsEmpty());                      \
    input_stream_preprocessor_.Advance(input_, cc); \
    goto state_name;                                \
  } while (false)
#define WEBVTT_SWITCH_TO(state_name)             \
  do {                                           \
    state = state_name;                          \
    DCHECK(!input_.IsEmpty());                   \
    input_stream_preprocessor_.Peek(input_, cc); \
    goto state_name;                             \
  } while (false)

static void AddNewClass(StringBuilder& classes,
                        const StringBuilder& new_class) {
  if (!classes.empty())
    classes.Append(' ');
  classes.Append(new_class);
}

inline bool EmitToken(VTTToken& result_token, const VTTToken& token) {
  result_token = token;
  return true;
}

inline bool AdvanceAndEmitToken(SegmentedString& source,
                                VTTToken& result_token,
                                const VTTToken& token) {
  source.AdvanceAndUpdateLineNumber();
  return EmitToken(result_token, token);
}

static void ProcessEntity(SegmentedString& source,
                          StringBuilder& result,
                          UChar additional_allowed_character = '\0') {
  bool not_enough_characters = false;
  DecodedHTMLEntity decoded_entity;
  bool success =
      ConsumeHTMLEntity(source, decoded_entity, not_enough_characters,
                        additional_allowed_character);
  if (not_enough_characters) {
    result.Append('&');
  } else if (!success) {
    DCHECK(decoded_entity.IsEmpty());
    result.Append('&');
  } else {
    for (unsigned i = 0; i < decoded_entity.length; ++i)
      result.Append(decoded_entity.data[i]);
  }
}

VTTTokenizer::VTTTokenizer(const String& input)
    : input_(input), input_stream_preprocessor_(this) {
  // Append a EOF marker and close the input "stream".
  DCHECK(!input_.IsClosed());
  input_.Append(SegmentedString(String(&kEndOfFileMarker, 1)));
  input_.Close();
}

bool VTTTokenizer::NextToken(VTTToken& token) {
  UChar cc;
  if (input_.IsEmpty() || !input_stream_preprocessor_.Peek(input_, cc))
    return false;

  if (cc == kEndOfFileMarker) {
    input_stream_preprocessor_.Advance(input_, cc);
    return false;
  }

  StringBuilder buffer;
  StringBuilder result;
  StringBuilder classes;
  enum {
    kDataState,
    kHTMLCharacterReferenceInDataState,
    kTagState,
    kStartTagState,
    kStartTagClassState,
    kStartTagAnnotationState,
    kHTMLCharacterReferenceInAnnotationState,
    kEndTagState,
    kTimestampTagState,
  } state = kDataState;

  // 4.8.10.13.4 WebVTT cue text tokenizer
  switch (state) {
    WEBVTT_BEGIN_STATE(kDataState) {
      if (cc == '&') {
        WEBVTT_ADVANCE_TO(kHTMLCharacterReferenceInDataState);
      } else if (cc == '<') {
        if (result.empty()) {
          WEBVTT_ADVANCE_TO(kTagState);
        } else {
          // We don't want to advance input or perform a state transition - just
          // return a (new) token.  (On the next call to nextToken we will see
          // '<' again, but take the other branch in this if instead.)
          return EmitToken(token, VTTToken::StringToken(result.ToString()));
        }
      } else if (cc == kEndOfFileMarker) {
        return AdvanceAndEmitToken(input_, token,
                                   VTTToken::StringToken(result.ToString()));
      } else {
        result.Append(cc);
        WEBVTT_ADVANCE_TO(kDataState);
      }
    }
    END_STATE()

    WEBVTT_BEGIN_STATE(kHTMLCharacterReferenceInDataState) {
      ProcessEntity(input_, result);
      WEBVTT_SWITCH_TO(kDataState);
    }
    END_STATE()

    WEBVTT_BEGIN_STATE(kTagState) {
      if (IsTokenizerWhitespace(cc)) {
        DCHECK(result.empty());
        WEBVTT_ADVANCE_TO(kStartTagAnnotationState);
      } else if (cc == '.') {
        DCHECK(result.empty());
        WEBVTT_ADVANCE_TO(kStartTagClassState);
      } else if (cc == '/') {
        WEBVTT_ADVANCE_TO(kEndTagState);
      } else if (WTF::IsASCIIDigit(cc)) {
        result.Append(cc);
        WEBVTT_ADVANCE_TO(kTimestampTagState);
      } else if (cc == '>' || cc == kEndOfFileMarker) {
        DCHECK(result.empty());
        return AdvanceAndEmitToken(input_, token,
                                   VTTToken::StartTag(result.ToString()));
      } else {
        result.Append(cc);
        WEBVTT_ADVANCE_TO(kStartTagState);
      }
    }
    END_STATE()

    WEBVTT_BEGIN_STATE(kStartTagState) {
      if (IsTokenizerWhitespace(cc)) {
        WEBVTT_ADVANCE_TO(kStartTagAnnotationState);
      } else if (cc == '.') {
        WEBVTT_ADVANCE_TO(kStartTagClassState);
      } else if (cc == '>' || cc == kEndOfFileMarker) {
        return AdvanceAndEmitToken(input_, token,
                                   VTTToken::StartTag(result.ToString()));
      } else {
        result.Append(cc);
        WEBVTT_ADVANCE_TO(kStartTagState);
      }
    }
    END_STATE()

    WEBVTT_BEGIN_STATE(kStartTagClassState) {
      if (IsTokenizerWhitespace(cc)) {
        AddNewClass(classes, buffer);
        buffer.Clear();
        WEBVTT_ADVANCE_TO(kStartTagAnnotationState);
      } else if (cc == '.') {
        AddNewClass(classes, buffer);
        buffer.Clear();
        WEBVTT_ADVANCE_TO(kStartTagClassState);
      } else if (cc == '>' || cc == kEndOfFileMarker) {
        AddNewClass(classes, buffer);
        buffer.Clear();
        return AdvanceAndEmitToken(
            input_, token,
            VTTToken::StartTag(result.ToString(), classes.ToAtomicString()));
      } else {
        buffer.Append(cc);
        WEBVTT_ADVANCE_TO(kStartTagClassState);
      }
    }
    END_STATE()

    WEBVTT_BEGIN_STATE(kStartTagAnnotationState) {
      if (cc == '&') {
        WEBVTT_ADVANCE_TO(kHTMLCharacterReferenceInAnnotationState);
      }
      if (cc == '>' || cc == kEndOfFileMarker) {
        return AdvanceAndEmitToken(
            input_, token,
            VTTToken::StartTag(result.ToString(), classes.ToAtomicString(),
                               buffer.ToAtomicString()));
      }
      buffer.Append(cc);
      WEBVTT_ADVANCE_TO(kStartTagAnnotationState);
    }
    END_STATE()

    WEBVTT_BEGIN_STATE(kHTMLCharacterReferenceInAnnotationState) {
      ProcessEntity(input_, buffer, '>');
      WEBVTT_SWITCH_TO(kStartTagAnnotationState);
    }
    END_STATE()

    WEBVTT_BEGIN_STATE(kEndTagState) {
      if (cc == '>' || cc == kEndOfFileMarker)
        return AdvanceAndEmitToken(input_, token,
                                   VTTToken::EndTag(result.ToString()));
      result.Append(cc);
      WEBVTT_ADVANCE_TO(kEndTagState);
    }
    END_STATE()

    WEBVTT_BEGIN_STATE(kTimestampTagState) {
      if (cc == '>' || cc == kEndOfFileMarker)
        return AdvanceAndEmitToken(input_, token,
                                   VTTToken::TimestampTag(result.ToString()));
      result.Append(cc);
      WEBVTT_ADVANCE_TO(kTimestampTagState);
    }
    END_STATE()
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace blink
