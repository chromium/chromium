// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/multipart_parser.h"

#include <algorithm>
#include <utility>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

constexpr char kCloseDelimiterSuffix[] = "--\r\n";
constexpr size_t kCloseDelimiterSuffixSize =
    std::size(kCloseDelimiterSuffix) - 1u;
constexpr size_t kDashBoundaryOffset = 2u;  // The length of "\r\n".
constexpr char kDelimiterSuffix[] = "\r\n";
constexpr size_t kDelimiterSuffixSize = std::size(kDelimiterSuffix) - 1u;

}  // namespace

MultipartParser::Matcher::Matcher() = default;

MultipartParser::Matcher::Matcher(const char* data,
                                  size_t num_matched_bytes,
                                  size_t size)
    : data_(data), num_matched_bytes_(num_matched_bytes), size_(size) {}

bool MultipartParser::Matcher::Match(const char* first, const char* last) {
  while (first < last) {
    if (!Match(*first++))
      return false;
  }
  return true;
}

void MultipartParser::Matcher::SetNumMatchedBytes(size_t num_matched_bytes) {
  DCHECK_LE(num_matched_bytes, size_);
  num_matched_bytes_ = num_matched_bytes;
}

MultipartParser::MultipartParser(Vector<char> boundary, Client* client)
    : client_(client),
      delimiter_(std::move(boundary)),
      state_(State::kParsingPreamble) {
  // The delimiter consists of "\r\n" and a dash boundary which consists of
  // "--" and a boundary.
  delimiter_.push_front("\r\n--", 4u);
  matcher_ = DelimiterMatcher(kDashBoundaryOffset);
}

bool MultipartParser::AppendData(const char* bytes, size_t size) {
  DCHECK_NE(State::kFinished, state_);
  DCHECK_NE(State::kCancelled, state_);

  const char* const bytes_end = bytes + size;

  while (bytes < bytes_end) {
    switch (state_) {
      case State::kParsingPreamble:
        // Parse either a preamble and a delimiter or a dash boundary.
        ParseDelimiter(&bytes, bytes_end);
        if (!matcher_.IsMatchComplete() && bytes < bytes_end) {
          // Parse a preamble data (by ignoring it) and then a delimiter.
          matcher_.SetNumMatchedBytes(0u);
          ParseDataAndDelimiter(&bytes, bytes_end);
        }
        if (matcher_.IsMatchComplete()) {
          // Prepare for a delimiter suffix.
          matcher_ = DelimiterSuffixMatcher();
          state_ = State::kParsingDelimiterSuffix;
        }
        break;

      case State::kParsingDelimiterSuffix:
        // Parse transport padding and "\r\n" after a delimiter.
        // This state can be reached after either a preamble or part
        // octets are parsed.
        if (matcher_.NumMatchedBytes() == 0u)
          ParseTransportPadding(&bytes, bytes_end);
        while (bytes < bytes_end) {
          if (!matcher_.Match(*bytes++))
            return false;
          if (matcher_.IsMatchComplete()) {
            // Prepare for part header fields.
            state_ = State::kParsingPartHeaderFields;
            break;
          }
        }
        break;

      case State::kParsingPartHeaderFields: {
        // Parse part header fields (which ends with "\r\n") and an empty
        // line (which also ends with "\r\n").
        // This state can be reached after a delimiter and a delimiter
        // suffix after either a preamble or part octets are parsed.
        HTTPHeaderMap header_fields;
        if (ParseHeaderFields(&bytes, bytes_end, &header_fields)) {
          // Prepare for part octets.
          matcher_ = DelimiterMatcher();
          state_ = State::kParsingPartOctets;
          client_->PartHeaderFieldsInMultipartReceived(header_fields);
        }
        break;
      }

      case State::kParsingPartOctets: {
        // Parse part octets and a delimiter.
        // This state can be reached only after part header fields are
        // parsed.
        const size_t num_initially_matched_bytes = matcher_.NumMatchedBytes();
        const char* octets_begin = bytes;
        ParseDelimiter(&bytes, bytes_end);
        if (!matcher_.IsMatchComplete() && bytes < bytes_end) {
          if (matcher_.NumMatchedBytes() >= num_initially_matched_bytes &&
              num_initially_matched_bytes > 0u) {
            // Since the matched bytes did not form a complete
            // delimiter, the matched bytes turned out to be octet
            // bytes instead of being delimiter bytes. Additionally,
            // some of the matched bytes are from the previous call and
            // are therefore not in the range [octetsBegin, bytesEnd[.
            client_->PartDataInMultipartReceived(matcher_.Data(),
                                                 matcher_.NumMatchedBytes());
            if (state_ != State::kParsingPartOctets)
              break;
            octets_begin = bytes;
          }
          matcher_.SetNumMatchedBytes(0u);
          ParseDataAndDelimiter(&bytes, bytes_end);
          const char* const octets_end = bytes - matcher_.NumMatchedBytes();
          if (octets_begin < octets_end) {
            client_->PartDataInMultipartReceived(
                octets_begin, static_cast<size_t>(octets_end - octets_begin));
            if (state_ != State::kParsingPartOctets)
              break;
          }
        }
        if (matcher_.IsMatchComplete()) {
          state_ = State::kParsingDelimiterOrCloseDelimiterSuffix;
          client_->PartDataInMultipartFullyReceived();
        }
        break;
      }

      case State::kParsingDelimiterOrCloseDelimiterSuffix:
        // Determine whether this is a delimiter suffix or a close
        // delimiter suffix.
        // This state can be reached only after part octets are parsed.
        if (*bytes == '-') {
          // Prepare for a close delimiter suffix.
          matcher_ = CloseDelimiterSuffixMatcher();
          state_ = State::kParsingCloseDelimiterSuffix;
        } else {
          // Prepare for a delimiter suffix.
          matcher_ = DelimiterSuffixMatcher();
          state_ = State::kParsingDelimiterSuffix;
        }
        break;

      case State::kParsingCloseDelimiterSuffix:
        // Parse "--", transport padding and "\r\n" after a delimiter
        // (a delimiter and "--" constitute a close delimiter).
        // This state can be reached only after part octets are parsed.
        for (;;) {
          if (matcher_.NumMatchedBytes() == 2u)
            ParseTransportPadding(&bytes, bytes_end);
          if (bytes >= bytes_end)
            break;
          if (!matcher_.Match(*bytes++))
            return false;
          if (matcher_.IsMatchComplete()) {
            // Prepare for an epilogue.
            state_ = State::kParsingEpilogue;
            break;
          }
        }
        break;

      case State::kParsingEpilogue:
        // Parse an epilogue (by ignoring it).
        // This state can be reached only after a delimiter and a close
        // delimiter suffix after part octets are parsed.
        return true;

      case State::kCancelled:
      case State::kFinished:
        // The client changed the state.
        return false;
    }
  }

  DCHECK_EQ(bytes_end, bytes);

  return true;
}

void MultipartParser::Cancel() {
  state_ = State::kCancelled;
}

bool MultipartParser::Finish() {
  DCHECK_NE(State::kCancelled, state_);
  DCHECK_NE(State::kFinished, state_);

  const State initial_state = state_;
  state_ = State::kFinished;

  switch (initial_state) {
    case State::kParsingPartOctets:
      if (matcher_.NumMatchedBytes() > 0u) {
        // Since the matched bytes did not form a complete delimiter,
        // the matched bytes turned out to be octet bytes instead of being
        // delimiter bytes.
        client_->PartDataInMultipartReceived(matcher_.Data(),
                                             matcher_.NumMatchedBytes());
      }
      return false;
    case State::kParsingCloseDelimiterSuffix:
      // Require a full close delimiter consisting of a delimiter and "--"
      // but ignore missing or partial "\r\n" after that.
      return matcher_.NumMatchedBytes() >= 2u;
    case State::kParsingEpilogue:
      return true;
    default:
      return false;
  }
}

MultipartParser::Matcher MultipartParser::CloseDelimiterSuffixMatcher() const {
  return Matcher(kCloseDelimiterSuffix, 0u, kCloseDelimiterSuffixSize);
}

MultipartParser::Matcher MultipartParser::DelimiterMatcher(
    size_t num_already_matched_bytes) const {
  return Matcher(delimiter_.data(), num_already_matched_bytes,
                 delimiter_.size());
}

MultipartParser::Matcher MultipartParser::DelimiterSuffixMatcher() const {
  return Matcher(kDelimiterSuffix, 0u, kDelimiterSuffixSize);
}

void MultipartParser::ParseDataAndDelimiter(const char** bytes_pointer,
                                            const char* bytes_end) {
  DCHECK_EQ(0u, matcher_.NumMatchedBytes());

  // Search for a complete delimiter within the bytes.
  const char* delimiter_begin = std::search(
      *bytes_pointer, bytes_end, delimiter_.begin(), delimiter_.end());
  if (delimiter_begin != bytes_end) {
    // A complete delimiter was found. The bytes before that are octet
    // bytes.
    const char* const delimiter_end = delimiter_begin + delimiter_.size();
    const bool matched = matcher_.Match(delimiter_begin, delimiter_end);
    DCHECK(matched);
    DCHECK(matcher_.IsMatchComplete());
    *bytes_pointer = delimiter_end;
  } else {
    // Search for a partial delimiter in the end of the bytes.
    const size_t size = static_cast<size_t>(bytes_end - *bytes_pointer);
    for (delimiter_begin =
             bytes_end -
             std::min(static_cast<size_t>(delimiter_.size() - 1u), size);
         delimiter_begin < bytes_end; ++delimiter_begin) {
      if (matcher_.Match(delimiter_begin, bytes_end))
        break;
      matcher_.SetNumMatchedBytes(0u);
    }
    // If a partial delimiter was found in the end of bytes, the bytes
    // before the partial delimiter are definitely octets bytes and
    // the partial delimiter bytes are buffered for now.
    // If a partial delimiter was not found in the end of bytes, all bytes
    // are definitely octets bytes.
    // In all cases, all bytes are parsed now.
    *bytes_pointer = bytes_end;
  }

  DCHECK(matcher_.IsMatchComplete() || *bytes_pointer == bytes_end);
}

void MultipartParser::ParseDelimiter(const char** bytes_pointer,
                                     const char* bytes_end) {
  DCHECK(!matcher_.IsMatchComplete());
  while (*bytes_pointer < bytes_end && matcher_.Match(*(*bytes_pointer))) {
    ++(*bytes_pointer);
    if (matcher_.IsMatchComplete())
      break;
  }
}

bool MultipartParser::ParseHeaderFields(const char** bytes_pointer,
                                        const char* bytes_end,
                                        HTTPHeaderMap* header_fields) {
  // Combine the current bytes with buffered header bytes if needed.
  const char* header_bytes = *bytes_pointer;
  if ((bytes_end - *bytes_pointer) > std::numeric_limits<wtf_size_t>::max())
    return false;

  wtf_size_t header_size = static_cast<wtf_size_t>(bytes_end - *bytes_pointer);
  if (!buffered_header_bytes_.empty()) {
    buffered_header_bytes_.Append(header_bytes, header_size);
    header_bytes = buffered_header_bytes_.data();
    header_size = buffered_header_bytes_.size();
  }

  wtf_size_t end = 0u;
  if (!ParseMultipartFormHeadersFromBody(header_bytes, header_size,
                                         header_fields, &end)) {
    // Store the current header bytes for the next call unless that has
    // already been done.
    if (buffered_header_bytes_.empty())
      buffered_header_bytes_.Append(header_bytes, header_size);
    *bytes_pointer = bytes_end;
    return false;
  }
  buffered_header_bytes_.clear();
  *bytes_pointer = bytes_end - (header_size - end);

  return true;
}

void MultipartParser::ParseTransportPadding(const char** bytes_pointer,
                                            const char* bytes_end) const {
  while (*bytes_pointer < bytes_end &&
         (*(*bytes_pointer) == '\t' || *(*bytes_pointer) == ' '))
    ++(*bytes_pointer);
}

void MultipartParser::Trace(Visitor* visitor) const {
  visitor->Trace(client_);
}

}  // namespace blink
