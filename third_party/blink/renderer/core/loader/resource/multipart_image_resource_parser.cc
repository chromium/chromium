// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/multipart_image_resource_parser.h"

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

MultipartImageResourceParser::MultipartImageResourceParser(
    const ResourceResponse& response,
    const Vector<char>& boundary,
    Client* client)
    : original_response_(response), boundary_(boundary), client_(client) {
  // Some servers report a boundary prefixed with "--".  See
  // https://crbug.com/5786.
  if (boundary_.size() < 2 || boundary_[0] != '-' || boundary_[1] != '-')
    boundary_.push_front("--", 2);
}

void MultipartImageResourceParser::AppendData(base::span<const char> bytes) {
  DCHECK(!IsCancelled());
  // m_sawLastBoundary means that we've already received the final boundary
  // token. The server should stop sending us data at this point, but if it
  // does, we just throw it away.
  if (saw_last_boundary_)
    return;
  data_.AppendSpan(bytes);

  if (is_parsing_top_) {
    // Eat leading \r\n
    wtf_size_t pos = SkippableLength(data_, 0);
    // +2 for "--"
    if (data_.size() < boundary_.size() + 2 + pos) {
      // We don't have enough data yet to make a boundary token.  Just wait
      // until the next chunk of data arrives.
      return;
    }
    if (pos)
      data_.EraseAt(0, pos);

    // Some servers don't send a boundary token before the first chunk of
    // data.  We handle this case anyway (Gecko does too).
    if (base::span(data_).first(boundary_.size()) != base::span(boundary_)) {
      data_.push_front("\n", 1);
      data_.PrependVector(boundary_);
    }
    is_parsing_top_ = false;
  }

  // Headers
  if (is_parsing_headers_) {
    if (!ParseHeaders()) {
      // Get more data before trying again.
      return;
    }
    // Successfully parsed headers.
    is_parsing_headers_ = false;
    if (IsCancelled())
      return;
  }

  wtf_size_t boundary_position;
  while ((boundary_position = FindBoundary(data_, &boundary_)) != kNotFound) {
    // Strip out trailing \r\n characters in the buffer preceding the boundary
    // on the same lines as does Firefox.
    wtf_size_t data_size = boundary_position;
    if (boundary_position > 0 && data_[boundary_position - 1] == '\n') {
      data_size--;
      if (boundary_position > 1 && data_[boundary_position - 2] == '\r') {
        data_size--;
      }
    }
    if (data_size) {
      client_->MultipartDataReceived(
          base::as_byte_span(data_).first(data_size));
      if (IsCancelled())
        return;
    }
    wtf_size_t boundary_end_position = boundary_position + boundary_.size();
    if (boundary_end_position < data_.size() &&
        '-' == data_[boundary_end_position]) {
      // This was the last boundary so we can stop processing.
      saw_last_boundary_ = true;
      data_.clear();
      return;
    }

    // We can now throw out data up through the boundary
    data_.EraseAt(0, boundary_end_position);

    // Ok, back to parsing headers
    if (!ParseHeaders()) {
      is_parsing_headers_ = true;
      break;
    }
    if (IsCancelled())
      return;
  }

  // At this point, we should send over any data we have, but keep enough data
  // buffered to handle a boundary that may have been truncated. "+2" for CRLF,
  // as we may ignore the last CRLF.
  if (!is_parsing_headers_ && data_.size() > boundary_.size() + 2) {
    auto send_data =
        base::as_byte_span(data_).first(data_.size() - boundary_.size() - 2);
    client_->MultipartDataReceived(send_data);
    data_.EraseAt(0, send_data.size());
  }
}

void MultipartImageResourceParser::Finish() {
  DCHECK(!IsCancelled());
  if (saw_last_boundary_)
    return;
  // If we have any pending data and we're not in a header, go ahead and send
  // it to the client.
  if (!is_parsing_headers_ && !data_.empty()) {
    client_->MultipartDataReceived(base::as_byte_span(data_));
  }
  data_.clear();
  saw_last_boundary_ = true;
}

wtf_size_t MultipartImageResourceParser::SkippableLength(
    const Vector<char>& data,
    wtf_size_t pos) {
  if (data.size() >= pos + 2 && data[pos] == '\r' && data[pos + 1] == '\n')
    return 2;
  if (data.size() >= pos + 1 && data[pos] == '\n')
    return 1;
  return 0;
}

bool MultipartImageResourceParser::ParseHeaders() {
  // Eat leading \r\n
  wtf_size_t pos = SkippableLength(data_, 0);

  // Create a ResourceResponse based on the original set of headers + the
  // replacement headers. We only replace the same few headers that gecko does.
  // See netwerk/streamconv/converters/nsMultiMixedConv.cpp.
  ResourceResponse response(original_response_.CurrentRequestUrl());
  response.SetWasFetchedViaServiceWorker(
      original_response_.WasFetchedViaServiceWorker());
  response.SetType(original_response_.GetType());
  for (const auto& header : original_response_.HttpHeaderFields())
    response.AddHttpHeaderField(header.key, header.value);

  wtf_size_t end = 0;
  if (!ParseMultipartHeadersFromBody(base::as_byte_span(data_).subspan(pos),
                                     &response, &end)) {
    return false;
  }
  data_.EraseAt(0, end + pos);
  // Send the response!
  client_->OnePartInMultipartReceived(response);
  return true;
}

// Boundaries are supposed to be preceeded with --, but it looks like gecko
// doesn't require the dashes to exist.  See nsMultiMixedConv::FindToken.
wtf_size_t MultipartImageResourceParser::FindBoundary(const Vector<char>& data,
                                                      Vector<char>* boundary) {
  auto it = base::ranges::search(data, *boundary);
  if (it == data.end())
    return kNotFound;

  wtf_size_t boundary_position = static_cast<wtf_size_t>(it - data.begin());
  // Back up over -- for backwards compat
  // TODO(tc): Don't we only want to do this once?  Gecko code doesn't seem to
  // care.
  if (boundary_position >= 2) {
    if (data[boundary_position - 1] == '-' &&
        data[boundary_position - 2] == '-') {
      boundary_position -= 2;
      Vector<char> v(2, '-');
      v.AppendVector(*boundary);
      *boundary = v;
    }
  }
  return boundary_position;
}

void MultipartImageResourceParser::Trace(Visitor* visitor) const {
  visitor->Trace(client_);
}

}  // namespace blink
