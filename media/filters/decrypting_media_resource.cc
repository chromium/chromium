// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decrypting_media_resource.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "media/base/cdm_context.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/base/pipeline_status.h"
#include "media/filters/decrypting_demuxer_stream.h"

namespace media {

DecryptingMediaResource::DecryptingMediaResource(
    MediaResource* media_resource,
    CdmContext* cdm_context,
    MediaLog* media_log,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : media_resource_(media_resource),
      cdm_context_(cdm_context),
      media_log_(media_log),
      task_runner_(task_runner) {
  DCHECK(media_resource);
  DCHECK_EQ(MediaResource::STREAM, media_resource->GetType());
  DCHECK(cdm_context_);
  DCHECK(cdm_context_->GetDecryptor());
  DCHECK(cdm_context_->GetDecryptor()->CanAlwaysDecrypt());
  DCHECK(media_log_);
  DCHECK(task_runner->BelongsToCurrentThread());
}

DecryptingMediaResource::~DecryptingMediaResource() = default;

MediaResource::Type DecryptingMediaResource::GetType() const {
  DCHECK_EQ(MediaResource::STREAM, media_resource_->GetType());
  return MediaResource::STREAM;
}

std::vector<DemuxerStream*> DecryptingMediaResource::GetAllStreams() {
  if (streams_.size())
    return streams_;

  return media_resource_->GetAllStreams();
}

void DecryptingMediaResource::Initialize(InitCB init_cb, WaitingCB waiting_cb) {
  DCHECK(init_cb);

  auto streams = media_resource_->GetAllStreams();

  // Save the callback so that we can invoke it when the
  // DecryptingDemuxerStreams have finished initialization.
  init_cb_ = std::move(init_cb);
  num_dds_pending_init_ = streams.size();

  for (auto* stream : streams) {
    auto decrypting_demuxer_stream = std::make_unique<DecryptingDemuxerStream>(
        task_runner_, media_log_, waiting_cb);

    // DecryptingDemuxerStream always invokes the callback asynchronously so
    // that we have no reentrancy issues. "All public APIs and callbacks are
    // trampolined to the |task_runner_|."
    decrypting_demuxer_stream->Initialize(
        stream, cdm_context_,
        base::BindRepeating(
            &DecryptingMediaResource::OnDecryptingDemuxerInitialized,
            weak_factory_.GetWeakPtr()));

    streams_.push_back(decrypting_demuxer_stream.get());
    owned_streams_.push_back(std::move(decrypting_demuxer_stream));
  }
}

int DecryptingMediaResource::DecryptingDemuxerStreamCountForTesting() const {
  return owned_streams_.size();
}

void DecryptingMediaResource::OnDecryptingDemuxerInitialized(
    PipelineStatus status) {
  DVLOG(2) << __func__ << ": DecryptingDemuxerStream initialization ended "
           << "with the status: " << status;

  // Decrement the count of DecryptingDemuxerStreams that need to be
  // initialized.
  --num_dds_pending_init_;

  if (!init_cb_)
    return;

  if (status != PIPELINE_OK)
    std::move(init_cb_).Run(false);
  else if (num_dds_pending_init_ == 0)
    std::move(init_cb_).Run(true);
}

}  // namespace media
