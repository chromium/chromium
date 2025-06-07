// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Derived from:
//   mozilla/netwerk/protocol/http/src/nsHttpChunkedDecoder.cpp
// The license block is:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/http/http_chunked_decoder.h"

#include <algorithm>
#include <string_view>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "net/base/net_errors.h"

namespace net {

// Absurdly long size to avoid imposing a constraint on chunked encoding
// extensions.
const size_t HttpChunkedDecoder::kMaxLineBufLen = 16384;

HttpChunkedDecoder::HttpChunkedDecoder() = default;

int HttpChunkedDecoder::FilterBuf(base::span<uint8_t> buf) {
  size_t result = 0;
  while (buf.size() > 0) {
    if (chunk_remaining_ > 0) {
      size_t num =
          std::min(base::saturated_cast<size_t>(chunk_remaining_), buf.size());

      chunk_remaining_ -= num;
      result += num;
      buf = buf.subspan(num);

      // After each chunk's data there should be a CRLF.
      if (chunk_remaining_ == 0)
        chunk_terminator_remaining_ = true;
      continue;
    } else if (reached_eof_) {
      bytes_after_eof_ += buf.size();
      break;  // Done!
    }

    int bytes_consumed = ScanForChunkRemaining(buf);
    if (bytes_consumed < 0)
      return bytes_consumed; // Error

    base::span<const uint8_t> subspan =
        buf.subspan(base::checked_cast<size_t>(bytes_consumed));
    if (!subspan.empty()) {
      buf.copy_prefix_from(subspan);
    }
    buf = buf.first(subspan.size());
  }
  // TODO(Kelsen): the return type should become size_t.
  return base::checked_cast<int>(result);
}

int HttpChunkedDecoder::ScanForChunkRemaining(base::span<const uint8_t> buf) {
  int bytes_consumed = 0;

  size_t index_of_lf = base::as_string_view(buf).find('\n');
  if (index_of_lf != std::string_view::npos) {
    buf = buf.first(index_of_lf);
    // Eliminate a preceding CR.
    if (!buf.empty() && buf.back() == '\r') {
      buf = buf.first(buf.size() - 1u);
    }
    bytes_consumed = static_cast<int>(index_of_lf) + 1;

    // Make buf point to the full line buffer to parse.
    if (!line_buf_.empty()) {
      line_buf_.append(base::as_string_view(buf));
      buf = base::as_byte_span(line_buf_);
    }

    if (reached_last_chunk_) {
      if (!buf.empty()) {
        DVLOG(1) << "ignoring http trailer";
      } else {
        reached_eof_ = true;
      }
    } else if (chunk_terminator_remaining_) {
      if (!buf.empty()) {
        DLOG(ERROR) << "chunk data not terminated properly";
        return ERR_INVALID_CHUNKED_ENCODING;
      }
      chunk_terminator_remaining_ = false;
    } else if (!buf.empty()) {
      // Ignore any chunk-extensions.
      size_t index_of_semicolon = base::as_string_view(buf).find(';');
      if (index_of_semicolon != std::string_view::npos) {
        buf = buf.first(index_of_semicolon);
      }

      if (!ParseChunkSize(buf, &chunk_remaining_)) {
        DLOG(ERROR) << "Failed parsing HEX from: " << base::as_string_view(buf);
        return ERR_INVALID_CHUNKED_ENCODING;
      }

      if (chunk_remaining_ == 0)
        reached_last_chunk_ = true;
    } else {
      DLOG(ERROR) << "missing chunk-size";
      return ERR_INVALID_CHUNKED_ENCODING;
    }
    line_buf_.clear();
  } else {
    // Save the partial line; wait for more data.
    bytes_consumed = buf.size();

    // Ignore a trailing CR
    if (buf.back() == '\r') {
      buf = buf.first(buf.size() - 1);
    }

    if (line_buf_.length() + buf.size() > kMaxLineBufLen) {
      DLOG(ERROR) << "Chunked line length too long";
      return ERR_INVALID_CHUNKED_ENCODING;
    }

    line_buf_.append(base::as_string_view(buf));
  }
  return bytes_consumed;
}

// While the HTTP 1.1 specification defines chunk-size as 1*HEX
// some sites rely on more lenient parsing.
// http://www.yahoo.com/, for example, pads chunk-size with trailing spaces
// (0x20) to be 7 characters long, such as "819b   ".
//
// A comparison of browsers running on WindowsXP shows that
// they will parse the following inputs (egrep syntax):
//
// Let \X be the character class for a hex digit: [0-9a-fA-F]
//
//   RFC 7230: ^\X+$
//        IE7: ^\X+[^\X]*$
// Safari 3.1: ^[\t\r ]*\X+[\t ]*$
//  Firefox 3: ^[\t\f\v\r ]*[+]?(0x)?\X+[^\X]*$
// Opera 9.51: ^[\t\f\v ]*[+]?(0x)?\X+[^\X]*$
//
// Our strategy is to be as strict as possible, while not breaking
// known sites.
//
//         Us: ^\X+[ ]*$
bool HttpChunkedDecoder::ParseChunkSize(base::span<const uint8_t> buf,
                                        uint64_t* out) {
  // Strip trailing spaces
  while (!buf.empty() && buf.back() == ' ') {
    buf = buf.first(buf.size() - 1u);
  }

  // Be more restrictive than HexStringToInt64;
  // don't allow inputs with leading "-", "+", "0x", "0X"
  std::string_view chunk_size = base::as_string_view(buf);
  if (!std::ranges::all_of(chunk_size, base::IsHexDigit<char>)) {
    return false;
  }

  int64_t parsed_number;
  bool ok = base::HexStringToInt64(chunk_size, &parsed_number);
  if (ok && parsed_number >= 0) {
    *out = parsed_number;
    return true;
  }
  return false;
}

}  // namespace net
