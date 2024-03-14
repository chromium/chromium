// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/mojom/commit_data_request_mojom_traits.h"

#include <utility>
#include "mojo/public/cpp/base/byte_string_mojom_traits.h"

namespace mojo {
// static
bool StructTraits<tracing::mojom::ChunksToMoveDataView,
                  perfetto::CommitDataRequest::ChunksToMove>::
    Read(tracing::mojom::ChunksToMoveDataView data,
         perfetto::CommitDataRequest::ChunksToMove* out) {
  out->set_page(data.page());
  out->set_chunk(data.chunk());
  out->set_target_buffer(data.target_buffer());
  return true;
}

// static
bool StructTraits<tracing::mojom::ChunkPatchDataView,
                  perfetto::CommitDataRequest::ChunkToPatch::Patch>::
    Read(tracing::mojom::ChunkPatchDataView data,
         perfetto::CommitDataRequest::ChunkToPatch::Patch* out) {
  std::string data_str;
  if (!data.ReadData(&data_str)) {
    return false;
  }
  out->set_offset(data.offset());
  out->set_data(data_str);
  return true;
}

// static
bool StructTraits<tracing::mojom::ChunksToPatchDataView,
                  perfetto::CommitDataRequest::ChunkToPatch>::
    Read(tracing::mojom::ChunksToPatchDataView data,
         perfetto::CommitDataRequest::ChunkToPatch* out) {
  std::vector<perfetto::CommitDataRequest::ChunkToPatch::Patch> patches;
  if (!data.ReadPatches(&patches)) {
    return false;
  }
  out->set_target_buffer(data.target_buffer());
  out->set_writer_id(data.writer_id());
  out->set_chunk_id(data.chunk_id());
  for (auto&& patch : patches) {
    *out->add_patches() = std::move(patch);
  }
  out->set_has_more_patches(data.has_more_patches());
  return true;
}

// static
bool StructTraits<tracing::mojom::CommitDataRequestDataView,
                  perfetto::CommitDataRequest>::
    Read(tracing::mojom::CommitDataRequestDataView data,
         perfetto::CommitDataRequest* out) {
  std::vector<perfetto::CommitDataRequest::ChunkToPatch> patches;
  std::vector<perfetto::CommitDataRequest::ChunksToMove> moves;
  if (!data.ReadChunksToMove(&moves) || !data.ReadChunksToPatch(&patches)) {
    return false;
  }
  for (auto&& move : moves) {
    *out->add_chunks_to_move() = std::move(move);
  }
  for (auto&& patch : patches) {
    *out->add_chunks_to_patch() = std::move(patch);
  }
  out->set_flush_request_id(data.flush_request_id());
  return true;
}
}  // namespace mojo
