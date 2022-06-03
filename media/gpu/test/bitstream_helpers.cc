// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/bitstream_helpers.h"

namespace media {
namespace test {

// static
scoped_refptr<BitstreamProcessor::BitstreamRef>
BitstreamProcessor::BitstreamRef::Create(
    scoped_refptr<DecoderBuffer> buffer,
    const BitstreamBufferMetadata& metadata,
    int32_t id,
    base::OnceClosure release_cb) {
  return new BitstreamRef(std::move(buffer), metadata, id,
                          std::move(release_cb));
}

BitstreamProcessor::BitstreamRef::BitstreamRef(
    scoped_refptr<DecoderBuffer> buffer,
    const BitstreamBufferMetadata& metadata,
    int32_t id,
    base::OnceClosure release_cb)
    : buffer(std::move(buffer)),
      metadata(metadata),
      id(id),
      release_cb(std::move(release_cb)) {}

BitstreamProcessor::BitstreamRef::~BitstreamRef() {
  std::move(release_cb).Run();
}
}  // namespace test
}  // namespace media
