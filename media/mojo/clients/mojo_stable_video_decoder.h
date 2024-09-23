// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_STABLE_VIDEO_DECODER_H_
#define MEDIA_MOJO_CLIENTS_MOJO_STABLE_VIDEO_DECODER_H_

#include "base/containers/id_map.h"
#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/base/video_decoder.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/gfx/generic_shared_memory_id.h"

namespace media {

class FrameResource;
class GpuVideoAcceleratorFactories;
class MediaLog;
class OOPVideoDecoder;

extern const char kMojoStableVideoDecoderDecodeLatencyHistogram[];

// A MojoStableVideoDecoder is analogous to a MojoVideoDecoder but for the
// stable::mojom::StableVideoDecoder interface, so in essence, it's just an
// adapter from the VideoDecoder API to the stable::mojom::StableVideoDecoder
// API.
//
// Consistent with the VideoDecoder contract, this class may be constructed on
// any sequence A. After that, it may be used on another sequence B but it must
// continue to be used and destroyed on that same sequence B. More specifically,
// B must correspond to the |media_task_runner| passed in the constructor.
class MojoStableVideoDecoder final : public VideoDecoder {
 public:
  MojoStableVideoDecoder(
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      GpuVideoAcceleratorFactories* gpu_factories,
      MediaLog* media_log,
      mojo::PendingRemote<stable::mojom::StableVideoDecoder>
          pending_remote_decoder);
  MojoStableVideoDecoder(const MojoStableVideoDecoder&) = delete;
  MojoStableVideoDecoder& operator=(const MojoStableVideoDecoder&) = delete;
  ~MojoStableVideoDecoder() final;

  // Decoder implementation.
  bool IsPlatformDecoder() const final;
  bool SupportsDecryption() const final;

  // VideoDecoder implementation.
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) final;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) final;
  void Reset(base::OnceClosure closure) final;
  bool NeedsBitstreamConversion() const final;
  bool CanReadWithoutStalling() const final;
  int GetMaxDecodeRequests() const final;
  VideoDecoderType GetDecoderType() const final;

 private:
  class SharedImageHolder;

  // OOPVideoDecoder has a couple of important assumptions:
  //
  // 1) It needs to be constructed, used, and destroyed on the same sequence
  //    (despite the VideoDecoder contract).
  //
  // 2) It should only be initialized after a higher layer (in this case the
  //    MojoStableVideoDecoder) checks that the VideoDecoderConfig is supported.
  //
  // InitializeOnceSupportedConfigsAreKnown() allows us to make sure these
  // assumptions are met because a) we can lazily create the OOPVideoDecoder in
  // the initialization path, and b) we can validate the VideoDecoderConfig once
  // the supported configurations are known and before calling
  // OOPVideoDecoder::Initialize().
  void InitializeOnceSupportedConfigsAreKnown(
      const VideoDecoderConfig& config,
      bool low_delay,
      CdmContext* cdm_context,
      InitCB init_cb,
      const OutputCB& output_cb,
      const WaitingCB& waiting_cb,
      mojo::PendingRemote<stable::mojom::StableVideoDecoder>
          pending_remote_decoder);

  scoped_refptr<SharedImageHolder> CreateOrUpdateSharedImageForFrame(
      const scoped_refptr<FrameResource>& frame_resource);

  void UnregisterSharedImage(gfx::GenericSharedMemoryId frame_id);

  void OnFrameResourceDecoded(scoped_refptr<FrameResource> frame_resource);

  OOPVideoDecoder* oop_video_decoder();

  const OOPVideoDecoder* oop_video_decoder() const;

  // DecodeBuffer/VideoFrame timestamps for histogram/tracing purposes. Must be
  // large enough to account for any amount of frame reordering.
  base::LRUCache<int64_t, base::TimeTicks> timestamps_;

  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ptr<GpuVideoAcceleratorFactories> gpu_factories_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // We hold onto the MediaLog* and the mojo::PendingRemote passed to the
  // constructor so that we can lazily create the |oop_video_decoder_|. After
  // the |oop_video_decoder_| is created, these two members should become
  // invalid.
  raw_ptr<MediaLog> media_log_ GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::PendingRemote<stable::mojom::StableVideoDecoder> pending_remote_decoder_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // TODO(b/327268445): OOPVideoDecoder knows how to talk to a
  // stable::mojom::StableVideoDecoder. The main reason we don't use it directly
  // is that that's a component shared with the regular OOP-VD path and we need
  // some things on top of it for GTFO OOP-VD, e.g., outputting
  // gpu::Mailbox-backed VideoFrames instead of media::FrameResources. Instead
  // of changing OOPVideoDecoder to handle both paths, we use it here as a
  // delegate. Once we switch fully from regular OOP-VD to GTFO OOP-VD, we can
  // merge OOPVideoDecoder into MojoStableVideoDecoder and get rid of it.
  std::unique_ptr<VideoDecoder> oop_video_decoder_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // |shared_images_| caches SharedImages so that we can re-use them if
  // possible. The fact that we keep a reference to a SharedImageHolder
  // guarantees that the corresponding SharedImage lives for at least as long as
  // the OOPVideoDecoder knows about the corresponding buffer.
  base::IDMap</*V=*/scoped_refptr<SharedImageHolder>,
              /*K=*/gfx::GenericSharedMemoryId>
      shared_images_ GUARDED_BY_CONTEXT(sequence_checker_);

  OutputCB output_cb_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<MojoStableVideoDecoder> weak_this_factory_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_STABLE_VIDEO_DECODER_H_
