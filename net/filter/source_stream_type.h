// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FILTER_SOURCE_STREAM_TYPE_H_
#define NET_FILTER_SOURCE_STREAM_TYPE_H_

#include "base/containers/enum_set.h"

namespace net {

// The type of net::SourceStream.
// LINT.IfChange(SourceStreamType)
enum class SourceStreamType {
  kBrotli,
  kMinValue = kBrotli,
  kDeflate,
  kGzip,
  kZstd,
  kUnknown,
  kNone,
  kMaxValue = kNone,
};
// LINT.ThenChange(//services/network/public/mojom/source_type.mojom:SourceType)

using SourceStreamTypeSet = base::EnumSet<SourceStreamType,
                                          SourceStreamType::kMinValue,
                                          SourceStreamType::kMaxValue>;

}  // namespace net

#endif  // NET_FILTER_SOURCE_STREAM_TYPE_H_
