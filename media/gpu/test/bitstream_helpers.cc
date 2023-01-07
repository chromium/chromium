// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/bitstream_helpers.h"

#include "media/base/decoder_buffer.h"

namespace media {
namespace test {

// static
scoped_refptr<BitstreamProcessor::BitstreamRef>
BitstreamProcessor::BitstreamRef::Create(
    scoped_refptr<DecoderBuffer> buffer,
    const BitstreamBufferMetadata& metadata,
    int32_t id,
    base::TimeTicks source_timestamp,
    base::OnceClosure release_cb) {
  return new BitstreamRef(std::move(buffer), metadata, id, source_timestamp,
                          std::move(release_cb));
}

BitstreamProcessor::BitstreamRef::BitstreamRef(
    scoped_refptr<DecoderBuffer> buffer,
    const BitstreamBufferMetadata& metadata,
    int32_t id,
    base::TimeTicks source_timestamp,
    base::OnceClosure release_cb)
    : buffer(std::move(buffer)),
      metadata(metadata),
      id(id),
      source_timestamp(source_timestamp),
      release_cb(std::move(release_cb)) {}

BitstreamProcessor::BitstreamRef::~BitstreamRef() {
  std::move(release_cb).Run();
}
}  // namespace test
}  // namespace media
