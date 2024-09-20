// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/eventsource/event_source_parser.h"

#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/modules/eventsource/event_source.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

EventSourceParser::EventSourceParser(const AtomicString& last_event_id,
                                     Client* client)
    : id_(last_event_id),
      last_event_id_(last_event_id),
      client_(client),
      codec_(NewTextCodec(UTF8Encoding())) {}

void EventSourceParser::AddBytes(base::span<const char> bytes) {
  // A line consists of |m_line| followed by
  // |bytes[start..(next line break)]|.
  size_t start = 0;
  const unsigned char kBOM[] = {0xef, 0xbb, 0xbf};
  for (size_t i = 0; i < bytes.size() && !is_stopped_; ++i) {
    // As kBOM contains neither CR nor LF, we can think BOM and the line
    // break separately.
    if (is_recognizing_bom_ && line_.size() + (i - start) == std::size(kBOM)) {
      Vector<char> line = line_;
      line.AppendSpan(bytes.subspan(start, i - start));
      DCHECK_EQ(line.size(), std::size(kBOM));
      is_recognizing_bom_ = false;
      if (base::as_byte_span(line) == base::span(kBOM)) {
        start = i;
        line_.clear();
        continue;
      }
    }
    if (is_recognizing_crlf_ && bytes[i] == '\n') {
      // This is the latter part of "\r\n".
      is_recognizing_crlf_ = false;
      ++start;
      continue;
    }
    is_recognizing_crlf_ = false;
    if (bytes[i] == '\r' || bytes[i] == '\n') {
      line_.AppendSpan(bytes.subspan(start, i - start));
      ParseLine();
      line_.clear();
      start = i + 1;
      is_recognizing_crlf_ = bytes[i] == '\r';
      is_recognizing_bom_ = false;
    }
  }
  if (is_stopped_)
    return;
  line_.AppendSpan(bytes.subspan(start));
}

void EventSourceParser::ParseLine() {
  if (line_.size() == 0) {
    last_event_id_ = id_;
    // We dispatch an event when seeing an empty line.
    if (!data_.empty()) {
      DCHECK_EQ(data_[data_.size() - 1], '\n');
      String data = FromUTF8(base::span(data_).first(data_.size() - 1u));
      client_->OnMessageEvent(
          event_type_.empty() ? event_type_names::kMessage : event_type_, data,
          last_event_id_);
      data_.clear();
    }
    event_type_ = g_null_atom;
    return;
  }
  wtf_size_t field_name_end = line_.Find(':');
  wtf_size_t field_value_start;
  if (field_name_end == WTF::kNotFound) {
    field_name_end = line_.size();
    field_value_start = field_name_end;
  } else {
    field_value_start = field_name_end + 1;
    if (field_value_start < line_.size() && line_[field_value_start] == ' ') {
      ++field_value_start;
    }
  }
  String field_name = FromUTF8(base::span(line_).first(field_name_end));
  auto field_value = base::span(line_).subspan(field_value_start);
  if (field_name == "event") {
    event_type_ = AtomicString(FromUTF8(field_value));
    return;
  }
  if (field_name == "data") {
    data_.AppendSpan(field_value);
    data_.push_back('\n');
    return;
  }
  if (field_name == "id") {
    if (base::ranges::find(field_value, '\0') == field_value.end()) {
      id_ = AtomicString(FromUTF8(field_value));
    }
    return;
  }
  if (field_name == "retry") {
    const bool has_only_digits =
        base::ranges::all_of(field_value, IsASCIIDigit<char>);
    if (field_value.empty()) {
      client_->OnReconnectionTimeSet(EventSource::kDefaultReconnectDelay);
    } else if (has_only_digits) {
      bool ok;
      auto reconnection_time = FromUTF8(field_value).ToUInt64Strict(&ok);
      if (ok)
        client_->OnReconnectionTimeSet(reconnection_time);
    }
    return;
  }
  // Unrecognized field name. Ignore!
}

String EventSourceParser::FromUTF8(base::span<const char> chars) {
  return codec_->Decode(base::as_bytes(chars), WTF::FlushBehavior::kDataEOF);
}

void EventSourceParser::Trace(Visitor* visitor) const {
  visitor->Trace(client_);
}

}  // namespace blink
