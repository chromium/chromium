// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/demuxer_stream.h"

namespace media {

// static
const char* DemuxerStream::GetTypeName(Type type) {
  switch (type) {
    case DemuxerStream::AUDIO:
      return "audio";
    case DemuxerStream::VIDEO:
      return "video";
    case DemuxerStream::TEXT:
      return "text";
    case DemuxerStream::UNKNOWN:
      return "unknown";
  }
}

// static
const char* DemuxerStream::GetStatusName(Status status) {
  switch (status) {
    case DemuxerStream::kOk:
      return "okay";
    case DemuxerStream::kAborted:
      return "aborted";
    case DemuxerStream::kConfigChanged:
      return "config_changed";
    case DemuxerStream::kError:
      return "error";
  }
}

DemuxerStream::~DemuxerStream() = default;

// Most DemuxerStream implementations don't specify liveness. Returns unknown
// liveness by default.
DemuxerStream::Liveness DemuxerStream::liveness() const {
  return DemuxerStream::LIVENESS_UNKNOWN;
}

// Most DemuxerStream implementations don't need to convert bit stream.
// Do nothing by default.
void DemuxerStream::EnableBitstreamConverter() {}

}  // namespace media
