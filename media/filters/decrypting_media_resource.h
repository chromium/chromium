// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DECRYPTING_MEDIA_RESOURCE_H_
#define MEDIA_FILTERS_DECRYPTING_MEDIA_RESOURCE_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/media_resource.h"
#include "media/base/pipeline.h"

namespace media {

class CdmContext;
class DemuxerStream;
class DecryptingDemuxerStream;

// DecryptingMediaResource is a wrapper for a MediaResource implementation that
// provides decryption. It should only be created when:
// - The |media_resource| has type MediaResource::Type::kStream, and
// - The |cdm_context| has a Decryptor that always supports decrypt-only.
// Internally DecryptingDemuxerStreams will be created for all streams in
// |media_resource| and decrypt them into clear streams. These clear streams are
// then passed downstream to the rest of the media pipeline, which should no
// longer need to worry about decryption.
class MEDIA_EXPORT DecryptingMediaResource : public MediaResource {
 public:
  using InitCB = base::OnceCallback<void(bool success)>;

  DecryptingMediaResource(MediaResource* media_resource,
                          CdmContext* cdm_context,
                          MediaLog* media_log,
                          scoped_refptr<base::SequencedTaskRunner> task_runner);

  DecryptingMediaResource(const DecryptingMediaResource&) = delete;
  DecryptingMediaResource& operator=(const DecryptingMediaResource&) = delete;

  ~DecryptingMediaResource() override;

  // MediaResource implementation:
  MediaResource::Type GetType() const override;
  std::vector<DemuxerStream*> GetAllStreams() override;

  void Initialize(InitCB init_cb, WaitingCB waiting_cb_);

  // Returns the number of DecryptingDemuxerStreams that were created.
  virtual int DecryptingDemuxerStreamCountForTesting() const;

 private:
  void OnDecryptingDemuxerInitialized(PipelineStatus status);

  const raw_ptr<MediaResource> media_resource_;
  const raw_ptr<CdmContext> cdm_context_;
  const raw_ptr<MediaLog> media_log_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Number of DecryptingDemuxerStreams that have yet to be initialized.
  int num_dds_pending_init_ = 0;

  // |streams_| is the set of streams that this implementation does not own and
  // will be returned when GetAllStreams() is invoked. |owned_streams_| is the
  // set of DecryptingDemuxerStreams that we have created and own (i.e.
  // responsible for destructing).
  std::vector<DemuxerStream*> streams_;
  std::vector<std::unique_ptr<DecryptingDemuxerStream>> owned_streams_;

  // Called when the final DecryptingDemuxerStream has been initialized *or*
  // if one of the DecryptingDemuxerStreams failed to initialize correctly.
  InitCB init_cb_;
  base::WeakPtrFactory<DecryptingMediaResource> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECRYPTING_MEDIA_RESOURCE_H_
