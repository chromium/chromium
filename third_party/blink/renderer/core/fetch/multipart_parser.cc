// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/multipart_parser.h"

#include <algorithm>
#include <utility>

#include "base/containers/span.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

constexpr char kCloseDelimiterSuffix[] = "--\r\n";
constexpr size_t kDashBoundaryOffset = 2u;  // The length of "\r\n".
constexpr char kDelimiterSuffix[] = "\r\n";

}  // namespace

MultipartParser::Matcher::Matcher() = default;

MultipartParser::Matcher::Matcher(base::span<const char> match_data,
                                  size_t num_matched_bytes)
    : match_data_(match_data), num_matched_bytes_(num_matched_bytes) {}

bool MultipartParser::Matcher::Match(base::span<const char> data) {
  for (const char c : data) {
    if (!Match(c)) {
      return false;
    }
  }
  return true;
}

void MultipartParser::Matcher::SetNumMatchedBytes(size_t num_matched_bytes) {
  DCHECK_LE(num_matched_bytes, match_data_.size());
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

bool MultipartParser::AppendData(base::span<const char> bytes) {
  DCHECK_NE(State::kFinished, state_);
  DCHECK_NE(State::kCancelled, state_);

  while (!bytes.empty()) {
    switch (state_) {
      case State::kParsingPreamble:
        // Parse either a preamble and a delimiter or a dash boundary.
        ParseDelimiter(bytes);
        if (!matcher_.IsMatchComplete() && !bytes.empty()) {
          // Parse a preamble data (by ignoring it) and then a delimiter.
          matcher_.SetNumMatchedBytes(0u);
          ParseDataAndDelimiter(bytes);
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
        if (matcher_.NumMatchedBytes() == 0u) {
          ParseTransportPadding(bytes);
        }
        while (!bytes.empty()) {
          if (!matcher_.Match(bytes.front())) {
            return false;
          }
          bytes = bytes.subspan(1u);
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
        if (ParseHeaderFields(bytes, &header_fields)) {
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
        auto bytes_before = bytes;
        ParseDelimiter(bytes);
        if (!matcher_.IsMatchComplete() && !bytes.empty()) {
          if (matcher_.NumMatchedBytes() >= num_initially_matched_bytes &&
              num_initially_matched_bytes > 0u) {
            // Since the matched bytes did not form a complete
            // delimiter, the matched bytes turned out to be octet
            // bytes instead of being delimiter bytes. Additionally,
            // some of the matched bytes are from the previous call and
            // are therefore not in the `bytes_before` span.
            client_->PartDataInMultipartReceived(matcher_.MatchedData());
            if (state_ != State::kParsingPartOctets)
              break;
            bytes_before = bytes;
          }
          matcher_.SetNumMatchedBytes(0u);
          ParseDataAndDelimiter(bytes);

          const size_t skipped_size = bytes_before.size() - bytes.size();
          if (skipped_size > matcher_.NumMatchedBytes()) {
            size_t payload_size = skipped_size - matcher_.NumMatchedBytes();
            auto payload = bytes_before.first(payload_size);
            client_->PartDataInMultipartReceived(payload);
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
        if (bytes.front() == '-') {
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
          if (matcher_.NumMatchedBytes() == 2u) {
            ParseTransportPadding(bytes);
          }
          if (bytes.empty()) {
            break;
          }
          if (!matcher_.Match(bytes.front())) {
            return false;
          }
          bytes = bytes.subspan(1u);
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

  DCHECK(bytes.empty());
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
        client_->PartDataInMultipartReceived(matcher_.MatchedData());
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
  return Matcher(base::span_from_cstring(kCloseDelimiterSuffix), 0u);
}

MultipartParser::Matcher MultipartParser::DelimiterMatcher(
    size_t num_already_matched_bytes) const {
  return Matcher(delimiter_, num_already_matched_bytes);
}

MultipartParser::Matcher MultipartParser::DelimiterSuffixMatcher() const {
  return Matcher(base::span_from_cstring(kDelimiterSuffix), 0u);
}

void MultipartParser::ParseDataAndDelimiter(base::span<const char>& bytes) {
  DCHECK_EQ(0u, matcher_.NumMatchedBytes());

  // Search for a complete delimiter within the bytes.
  auto delimiter_begin = base::ranges::search(bytes, delimiter_);
  if (delimiter_begin != bytes.end()) {
    // A complete delimiter was found. The bytes before that are octet
    // bytes.
    auto delimiter_and_rest =
        bytes.subspan(std::distance(bytes.begin(), delimiter_begin));
    auto [delimiter, rest] = delimiter_and_rest.split_at(delimiter_.size());
    const bool matched = matcher_.Match(delimiter);
    DCHECK(matched);
    DCHECK(matcher_.IsMatchComplete());
    bytes = rest;
  } else {
    // Search for a partial delimiter in the end of the bytes.
    auto maybe_delimiter_span = bytes.last(
        std::min(static_cast<size_t>(delimiter_.size() - 1u), bytes.size()));
    while (!maybe_delimiter_span.empty()) {
      if (matcher_.Match(maybe_delimiter_span)) {
        break;
      }
      maybe_delimiter_span = maybe_delimiter_span.subspan(1u);
      matcher_.SetNumMatchedBytes(0u);
    }
    // If a partial delimiter was found in the end of bytes, the bytes
    // before the partial delimiter are definitely octets bytes and
    // the partial delimiter bytes are buffered for now.
    // If a partial delimiter was not found in the end of bytes, all bytes
    // are definitely octets bytes.
    // In all cases, all bytes are parsed now.
    bytes = {};
  }

  DCHECK(matcher_.IsMatchComplete() || bytes.empty());
}

void MultipartParser::ParseDelimiter(base::span<const char>& bytes) {
  DCHECK(!matcher_.IsMatchComplete());
  size_t matched = 0;
  while (matched < bytes.size() && matcher_.Match(bytes[matched])) {
    ++matched;
    if (matcher_.IsMatchComplete())
      break;
  }
  bytes = bytes.subspan(matched);
}

bool MultipartParser::ParseHeaderFields(base::span<const char>& bytes,
                                        HTTPHeaderMap* header_fields) {
  // Combine the current bytes with buffered header bytes if needed.
  if (bytes.size() > std::numeric_limits<wtf_size_t>::max()) {
    return false;
  }

  auto header_bytes = bytes;
  if (!buffered_header_bytes_.empty()) {
    buffered_header_bytes_.Append(
        header_bytes.data(),
        base::checked_cast<wtf_size_t>(header_bytes.size()));
    header_bytes = buffered_header_bytes_;
  }

  wtf_size_t end = 0u;
  if (!ParseMultipartFormHeadersFromBody(base::as_bytes(header_bytes),
                                         header_fields, &end)) {
    // Store the current header bytes for the next call unless that has
    // already been done.
    if (buffered_header_bytes_.empty()) {
      buffered_header_bytes_.Append(
          header_bytes.data(),
          base::checked_cast<wtf_size_t>(header_bytes.size()));
    }
    bytes = {};
    return false;
  }
  buffered_header_bytes_.clear();
  bytes = bytes.last(header_bytes.size() - end);
  return true;
}

void MultipartParser::ParseTransportPadding(
    base::span<const char>& bytes) const {
  size_t matched = 0;
  while (matched < bytes.size() &&
         (bytes[matched] == '\t' || bytes[matched] == ' ')) {
    ++matched;
  }
  bytes = bytes.subspan(matched);
}

void MultipartParser::Trace(Visitor* visitor) const {
  visitor->Trace(client_);
}

}  // namespace blink
