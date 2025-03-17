// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FILTER_SOURCE_STREAM_TYPE_H_
#define NET_FILTER_SOURCE_STREAM_TYPE_H_

namespace net {

// The type of net::SourceStream.
// LINT.IfChange(SourceStreamType)
enum class SourceStreamType {
  kBrotli,
  kDeflate,
  kGzip,
  kZstd,
  kUnknown,
  kNone,
};
// LINT.ThenChange(//services/network/public/mojom/source_type.mojom:SourceType)

}  // namespace net

#endif  // NET_FILTER_SOURCE_STREAM_TYPE_H_
