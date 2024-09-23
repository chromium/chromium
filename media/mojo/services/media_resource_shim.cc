// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_resource_shim.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"

namespace media {

MediaResourceShim::MediaResourceShim(
    std::vector<mojo::PendingRemote<mojom::DemuxerStream>> streams,
    base::OnceClosure demuxer_ready_cb)
    : demuxer_ready_cb_(std::move(demuxer_ready_cb)), streams_ready_(0) {
  DCHECK(!streams.empty());
  DCHECK(demuxer_ready_cb_);

  for (auto& s : streams) {
    streams_.emplace_back(std::make_unique<MojoDemuxerStreamAdapter>(
        std::move(s), base::BindOnce(&MediaResourceShim::OnStreamReady,
                                     weak_factory_.GetWeakPtr())));
  }
}

MediaResourceShim::~MediaResourceShim() = default;

std::vector<DemuxerStream*> MediaResourceShim::GetAllStreams() {
  DCHECK(!demuxer_ready_cb_);
  std::vector<DemuxerStream*> result;
  for (auto& stream : streams_) {
    result.push_back(stream.get());
  }
  return result;
}

void MediaResourceShim::OnStreamReady() {
  if (++streams_ready_ == streams_.size())
    std::move(demuxer_ready_cb_).Run();
}

}  // namespace media
