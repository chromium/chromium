// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/eventsource/event_source_parser.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/modules/eventsource/event_source.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
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

void EventSourceParser::AddBytes(const char* bytes, uint32_t size) {
  // A line consists of |m_line| followed by
  // |bytes[start..(next line break)]|.
  uint32_t start = 0;
  const unsigned char kBOM[] = {0xef, 0xbb, 0xbf};
  for (uint32_t i = 0; i < size && !is_stopped_; ++i) {
    // As kBOM contains neither CR nor LF, we can think BOM and the line
    // break separately.
    if (is_recognizing_bom_ && line_.size() + (i - start) == base::size(kBOM)) {
      Vector<char> line = line_;
      line.Append(&bytes[start], i - start);
      DCHECK_EQ(line.size(), base::size(kBOM));
      is_recognizing_bom_ = false;
      if (memcmp(line.data(), kBOM, sizeof(kBOM)) == 0) {
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
      line_.Append(&bytes[start], i - start);
      ParseLine();
      line_.clear();
      start = i + 1;
      is_recognizing_crlf_ = bytes[i] == '\r';
      is_recognizing_bom_ = false;
    }
  }
  if (is_stopped_)
    return;
  line_.Append(&bytes[start], size - start);
}

void EventSourceParser::ParseLine() {
  if (line_.size() == 0) {
    last_event_id_ = id_;
    // We dispatch an event when seeing an empty line.
    if (!data_.IsEmpty()) {
      DCHECK_EQ(data_[data_.size() - 1], '\n');
      String data = FromUTF8(data_.data(), data_.size() - 1);
      client_->OnMessageEvent(
          event_type_.IsEmpty() ? event_type_names::kMessage : event_type_,
          data, last_event_id_);
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
  wtf_size_t field_value_size = line_.size() - field_value_start;
  String field_name = FromUTF8(line_.data(), field_name_end);
  if (field_name == "event") {
    event_type_ = AtomicString(
        FromUTF8(line_.data() + field_value_start, field_value_size));
    return;
  }
  if (field_name == "data") {
    data_.Append(line_.data() + field_value_start, field_value_size);
    data_.push_back('\n');
    return;
  }
  if (field_name == "id") {
    if (!memchr(line_.data() + field_value_start, '\0', field_value_size)) {
      id_ = AtomicString(
          FromUTF8(line_.data() + field_value_start, field_value_size));
    }
    return;
  }
  if (field_name == "retry") {
    bool has_only_digits = true;
    for (wtf_size_t i = field_value_start; i < line_.size() && has_only_digits;
         ++i)
      has_only_digits = IsASCIIDigit(line_[i]);
    if (field_value_start == line_.size()) {
      client_->OnReconnectionTimeSet(EventSource::kDefaultReconnectDelay);
    } else if (has_only_digits) {
      bool ok;
      auto reconnection_time =
          FromUTF8(line_.data() + field_value_start, field_value_size)
              .ToUInt64Strict(&ok);
      if (ok)
        client_->OnReconnectionTimeSet(reconnection_time);
    }
    return;
  }
  // Unrecognized field name. Ignore!
}

String EventSourceParser::FromUTF8(const char* bytes, uint32_t size) {
  return codec_->Decode(bytes, size, WTF::FlushBehavior::kDataEOF);
}

void EventSourceParser::Trace(blink::Visitor* visitor) {
  visitor->Trace(client_);
}

}  // namespace blink
