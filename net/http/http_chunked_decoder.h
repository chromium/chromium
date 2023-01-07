// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Derived from:
//   mozilla/netwerk/protocol/http/src/nsHttpChunkedDecoder.h
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

#ifndef NET_HTTP_HTTP_CHUNKED_DECODER_H_
#define NET_HTTP_HTTP_CHUNKED_DECODER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "net/base/net_export.h"

namespace net {

// From RFC2617 section 3.6.1, the chunked transfer coding is defined as:
//
//   Chunked-Body    = *chunk
//                     last-chunk
//                     trailer
//                     CRLF
//   chunk           = chunk-size [ chunk-extension ] CRLF
//                     chunk-data CRLF
//   chunk-size      = 1*HEX
//   last-chunk      = 1*("0") [ chunk-extension ] CRLF
//
//   chunk-extension = *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
//   chunk-ext-name  = token
//   chunk-ext-val   = token | quoted-string
//   chunk-data      = chunk-size(OCTET)
//   trailer         = *(entity-header CRLF)
//
// The chunk-size field is a string of hex digits indicating the size of the
// chunk.  The chunked encoding is ended by any chunk whose size is zero,
// followed by the trailer, which is terminated by an empty line.
//
// NOTE: This implementation does not bother to parse trailers since they are
// not used on the web.
//
class NET_EXPORT_PRIVATE HttpChunkedDecoder {
 public:
  // The maximum length of |line_buf_| between calls to FilterBuff().
  // Exposed for tests.
  static const size_t kMaxLineBufLen;

  HttpChunkedDecoder();

  // Indicates that a previous call to FilterBuf encountered the final CRLF.
  bool reached_eof() const { return reached_eof_; }

  // Returns the number of bytes after the final CRLF.
  int bytes_after_eof() const { return bytes_after_eof_; }

  // Called to filter out the chunk markers from buf and to check for end-of-
  // file.  This method modifies |buf| inline if necessary to remove chunk
  // markers.  The return value indicates the final size of decoded data stored
  // in |buf|.  Call reached_eof() after this method to check if end-of-file
  // was encountered.
  int FilterBuf(char* buf, int buf_len);

 private:
  // Scans |buf| for the next chunk delimiter.  This method returns the number
  // of bytes consumed from |buf|.  If found, |chunk_remaining_| holds the
  // value for the next chunk size.
  int ScanForChunkRemaining(const char* buf, int buf_len);

  // Converts string |start| of length |len| to a numeric value.
  // |start| is a string of type "chunk-size" (hex string).
  // If the conversion succeeds, returns true and places the result in |out|.
  static bool ParseChunkSize(const char* start, int len, int64_t* out);

  // Indicates the number of bytes remaining for the current chunk.
  int64_t chunk_remaining_ = 0;

  // A small buffer used to store a partial chunk marker.
  std::string line_buf_;

  // True if waiting for the terminal CRLF of a chunk's data.
  bool chunk_terminator_remaining_ = false;

  // Set to true when FilterBuf encounters the last-chunk.
  bool reached_last_chunk_ = false;

  // Set to true when FilterBuf encounters the final CRLF.
  bool reached_eof_ = false;

  // The number of extraneous unfiltered bytes after the final CRLF.
  int bytes_after_eof_ = 0;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_CHUNKED_DECODER_H_
