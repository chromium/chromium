// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines mappings from mojom IPC representations to their native perfetto
// equivalents.

#ifndef SERVICES_TRACING_PUBLIC_MOJOM_COMMIT_DATA_REQUEST_MOJOM_TRAITS_H_
#define SERVICES_TRACING_PUBLIC_MOJOM_COMMIT_DATA_REQUEST_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/tracing/public/mojom/perfetto_service.mojom-shared.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/commit_data_request.h"

namespace mojo {

// ChunksToMove
template <>
class StructTraits<tracing::mojom::ChunksToMoveDataView,
                   perfetto::CommitDataRequest::ChunksToMove> {
 public:
  static uint32_t page(const perfetto::CommitDataRequest::ChunksToMove& src) {
    return src.page();
  }
  static uint32_t chunk(const perfetto::CommitDataRequest::ChunksToMove& src) {
    return src.chunk();
  }
  static uint32_t target_buffer(
      const perfetto::CommitDataRequest::ChunksToMove& src) {
    return src.target_buffer();
  }

  static bool Read(tracing::mojom::ChunksToMoveDataView data,
                   perfetto::CommitDataRequest::ChunksToMove* out);
};

// ChunkPatch
template <>
class StructTraits<tracing::mojom::ChunkPatchDataView,
                   perfetto::CommitDataRequest::ChunkToPatch::Patch> {
 public:
  static uint32_t offset(
      const perfetto::CommitDataRequest::ChunkToPatch::Patch& src) {
    return src.offset();
  }
  static const std::string& data(
      const perfetto::CommitDataRequest::ChunkToPatch::Patch& src) {
    return src.data();
  }

  static bool Read(tracing::mojom::ChunkPatchDataView data,
                   perfetto::CommitDataRequest::ChunkToPatch::Patch* out);
};

// ChunkToPatch
template <>
class StructTraits<tracing::mojom::ChunksToPatchDataView,
                   perfetto::CommitDataRequest::ChunkToPatch> {
 public:
  static uint32_t target_buffer(
      const perfetto::CommitDataRequest::ChunkToPatch& src) {
    return src.target_buffer();
  }
  static uint32_t writer_id(
      const perfetto::CommitDataRequest::ChunkToPatch& src) {
    return src.writer_id();
  }
  static uint32_t chunk_id(
      const perfetto::CommitDataRequest::ChunkToPatch& src) {
    return src.chunk_id();
  }
  static const std::vector<perfetto::CommitDataRequest::ChunkToPatch::Patch>&
  patches(const perfetto::CommitDataRequest::ChunkToPatch& src) {
    return src.patches();
  }

  static bool has_more_patches(
      const perfetto::CommitDataRequest::ChunkToPatch& src) {
    return src.has_more_patches();
  }

  static bool Read(tracing::mojom::ChunksToPatchDataView data,
                   perfetto::CommitDataRequest::ChunkToPatch* out);
};

// CommitDataRequest
template <>
class StructTraits<tracing::mojom::CommitDataRequestDataView,
                   perfetto::CommitDataRequest> {
 public:
  static const std::vector<perfetto::CommitDataRequest::ChunksToMove>&
  chunks_to_move(const perfetto::CommitDataRequest& src) {
    return src.chunks_to_move();
  }
  static const std::vector<perfetto::CommitDataRequest::ChunkToPatch>&
  chunks_to_patch(const perfetto::CommitDataRequest& src) {
    return src.chunks_to_patch();
  }
  static uint64_t flush_request_id(const perfetto::CommitDataRequest& src) {
    return src.flush_request_id();
  }

  static bool Read(tracing::mojom::CommitDataRequestDataView data,
                   perfetto::CommitDataRequest* out);
};
}  // namespace mojo
#endif  // SERVICES_TRACING_PUBLIC_MOJOM_COMMIT_DATA_REQUEST_MOJOM_TRAITS_H_
