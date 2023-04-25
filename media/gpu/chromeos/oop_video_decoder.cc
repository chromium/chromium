// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/oop_video_decoder.h"

#include "base/memory/ptr_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/components/cdm_factory_daemon/stable_cdm_context_impl.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/format_utils.h"
#include "media/gpu/buffer_validation.h"
#include "media/gpu/macros.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif  // BUILDFLAG(USE_VAAPI)

// Throughout this file, we have sprinkled many CHECK()s to assert invariants
// that should hold regardless of the behavior of the remote decoder or
// untrusted client. We use CHECK()s instead of DCHECK()s because
// OOPVideoDecoder and associated classes are very stateful so:
//
// a) They're hard to reason about.
// b) They're hard to fully exercise with tests.
// c) It's hard to reason if the violation of an invariant can have security
//    implications because once we enter into a bad state, everything is fair
//    game.
//
// Hence it's safer to crash and surface those crashes.
//
// More specifically:
//
// - It's illegal to call many methods if OOPVideoDecoder enters into an error
//   state (tracked by |has_error_|).
//
// - The media::VideoDecoder interface demands that its users don't call certain
//   methods while in specific states. An OOPVideoDecoder is used by an
//   in-process class (the VideoDecoderPipeline) to communicate with an
//   out-of-process video decoder. Therefore, we trust that the in-process user
//   of this class abides by the requirements of the media::VideoDecoder
//   interface and thus, we don't handle violations gracefully. In particular:
//
//   - No media::VideoDecoder methods should be called before the |init_cb|
//     passed to Initialize() is called. We track this interim state with
//     |init_cb_|.
//
//   - Initialize() should not be called while there are pending decodes (i.e.,
//     while !pending_decodes_.empty()).
//
//   - No media::VideoDecoder methods should be called before the |closure|
//     passed to Reset() is called. We track this interim state with
//     |reset_cb_|.

// TODO(b/220915557): OOPVideoDecoder cannot trust |remote_decoder_| (but
// |remote_decoder_| might trust us). We need to audit this class to make sure:
//
// - That OOPVideoDecoder validates everything coming from
//   |remote_video_decoder_|.
//
// - That OOPVideoDecoder meets the requirements of the media::VideoDecoder and
//   the media::VideoDecoderMixin interfaces. For example, we need to make sure
//   we guarantee statements like "all pending Decode() requests will be
//   finished or aborted before |closure| is called" (for
//   VideoDecoder::Reset()).
//
// - That OOPVideoDecoder asserts it's not being misused (which might cause us
//   to violate the requirements of the StableVideoDecoder interface). For
//   example, the StableVideoDecoder interface says for Decode(): "this must not
//   be called while there are pending Initialize(), Reset(), or Decode(EOS)
//   requests."

namespace media {

namespace {

// Size of the timestamp cache. We don't want the cache to grow without bounds.
// The maximum size is chosen to be the same as in the VaapiVideoDecoder.
constexpr size_t kTimestampCacheSize = 128;

// Converts |mojo_frame| to a media::VideoFrame after performing some
// validation. The reason we do validation/conversion here and not in mojo
// traits is that we don't want every incoming stable::mojom::VideoFrame to
// result in a media::VideoFrame: we'd like to re-use buffers based on the
// incoming |mojo_frame|->gpu_memory_buffer_handle.id; if that incoming
// |mojo_frame| is a frame that we already know about, we can reduce the
// underlying buffer without creating a media::VideoFrame.
//
// TODO(b/277832201): actually re-use buffers.
scoped_refptr<VideoFrame> MojoVideoFrameToMediaVideoFrame(
    stable::mojom::VideoFramePtr mojo_frame) {
  if (!VerifyGpuMemoryBufferHandle(mojo_frame->format, mojo_frame->coded_size,
                                   mojo_frame->gpu_memory_buffer_handle)) {
    VLOGF(2) << "Received an invalid GpuMemoryBufferHandle";
    return nullptr;
  }

  absl::optional<gfx::BufferFormat> buffer_format =
      VideoPixelFormatToGfxBufferFormat(mojo_frame->format);
  if (!buffer_format) {
    VLOGF(2) << "Could not convert the incoming frame's format to a "
                "gfx::BufferFormat";
    return nullptr;
  }

  gpu::GpuMemoryBufferSupport support;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      support.CreateGpuMemoryBufferImplFromHandle(
          std::move(mojo_frame->gpu_memory_buffer_handle),
          mojo_frame->coded_size, *buffer_format,
          gfx::BufferUsage::SCANOUT_VDA_WRITE, base::NullCallback());
  if (!gpu_memory_buffer) {
    VLOGF(2) << "Could not create a GpuMemoryBuffer for the incoming frame";
    return nullptr;
  }

  gpu::MailboxHolder dummy_mailbox[media::VideoFrame::kMaxPlanes];
  scoped_refptr<media::VideoFrame> gmb_frame =
      media::VideoFrame::WrapExternalGpuMemoryBuffer(
          mojo_frame->visible_rect, mojo_frame->natural_size,
          std::move(gpu_memory_buffer), dummy_mailbox, base::NullCallback(),
          mojo_frame->timestamp);
  if (!gmb_frame) {
    VLOGF(2) << "Could not create a GpuMemoryBuffer-backed VideoFrame";
    return nullptr;
  }

  gmb_frame->set_metadata(mojo_frame->metadata);
  gmb_frame->set_color_space(mojo_frame->color_space);
  gmb_frame->set_hdr_metadata(mojo_frame->hdr_metadata);

  return gmb_frame;
}

// A singleton helper class that makes it easy to manage requests to wait until
// the supported video decoder configurations are known and cache those
// configurations.
//
// All public methods are thread- and sequence-safe.
class OOPVideoDecoderSupportedConfigsManager {
 public:
  static OOPVideoDecoderSupportedConfigsManager& Instance() {
    static base::NoDestructor<OOPVideoDecoderSupportedConfigsManager> instance;
    return *instance;
  }

  absl::optional<SupportedVideoDecoderConfigs> Get() {
    base::AutoLock lock(lock_);
    return configs_;
  }

  VideoDecoderType GetDecoderType() {
    base::AutoLock lock(lock_);
    // This method should only be called in the initialization path of an
    // OOPVideoDecoder instance. OOPVideoDecoder instances are initialized only
    // after higher layers check that a VideoDecoderConfig is supported. If
    // |decoder_type_| is not initialized to non-nullopt, it means that we're in
    // one of two cases:
    //
    // a) We didn't try to get the supported configurations before initializing
    //    OOPVideoDecoder instances. This should be impossible as higher layers
    //    should guarantee that we know the supported configurations before
    //    creating MojoVideoDecoderService instances (and therefore
    //    OOPVideoDecoder instances). See, e.g., the logic in
    //    InterfaceFactoryImpl::CreateVideoDecoder().
    //
    // b) We did try to get the supported configurations but an error occurred.
    //    This case reduces to no supported configurations in which case, a
    //    higher layer should reject any initialization attempt.
    //
    // Therefore, GetDecoderType() should only be reached when |decoder_type_|
    // is known.
    CHECK(decoder_type_.has_value());
    return *decoder_type_;
  }

  void NotifySupportKnown(
      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder,
      base::OnceCallback<
          void(mojo::PendingRemote<stable::mojom::StableVideoDecoder>)> cb) {
    base::ReleasableAutoLock lock(&lock_);
    if (configs_) {
      // The supported configurations are already known. We can call |cb|
      // immediately.
      //
      // We release the lock in case the |waiting_callback|.cb wants to re-enter
      // OOPVideoDecoderSupportedConfigsManager by reaching
      // OOPVideoDecoderSupportedConfigsManager::Get() in the callback.
      lock.Release();
      std::move(cb).Run(std::move(oop_video_decoder));
      return;
    } else if (!waiting_callbacks_.empty()) {
      // There is a query in progress. We need to queue |cb| to call it later
      // when the supported configurations are known.
      waiting_callbacks_.emplace(
          std::move(oop_video_decoder), std::move(cb),
          base::SequencedTaskRunner::GetCurrentDefault());
      return;
    }

    // The supported configurations are not known. We need to use
    // |oop_video_decoder| to query them.
    //
    // Note: base::Unretained(this) is safe because the
    // OOPVideoDecoderSupportedConfigsManager never gets destroyed.
    oop_video_decoder_.Bind(std::move(oop_video_decoder));
    oop_video_decoder_.set_disconnect_handler(base::BindOnce(
        &OOPVideoDecoderSupportedConfigsManager::OnGetSupportedConfigs,
        base::Unretained(this), SupportedVideoDecoderConfigs(),
        VideoDecoderType::kUnknown));
    oop_video_decoder_->GetSupportedConfigs(base::BindOnce(
        &OOPVideoDecoderSupportedConfigsManager::OnGetSupportedConfigs,
        base::Unretained(this)));

    // Eventually, we need to call |cb|. We can't store |oop_video_decoder| here
    // because it's been taken over by the |oop_video_decoder_|. For now, we'll
    // store a default-constructed PendingRemote. Later, when we have to call
    // |cb|, we can pass |oop_video_decoder_|.Unbind().
    waiting_callbacks_.emplace(
        mojo::PendingRemote<stable::mojom::StableVideoDecoder>(), std::move(cb),
        base::SequencedTaskRunner::GetCurrentDefault());
  }

 private:
  friend class base::NoDestructor<OOPVideoDecoderSupportedConfigsManager>;

  OOPVideoDecoderSupportedConfigsManager() = default;
  ~OOPVideoDecoderSupportedConfigsManager() = default;

  void OnGetSupportedConfigs(const SupportedVideoDecoderConfigs& configs,
                             VideoDecoderType decoder_type) {
    base::AutoLock lock(lock_);
    DCHECK(!configs_);
    DCHECK(!decoder_type_);

    if (decoder_type == VideoDecoderType::kVda ||
        decoder_type == VideoDecoderType::kVaapi ||
        decoder_type == VideoDecoderType::kV4L2) {
      configs_ = configs;
      decoder_type_ = decoder_type;
    } else {
      // The remote decoder is of an unexpected type, so let's assume it's bad.
      configs_ = {};
    }

    while (!waiting_callbacks_.empty()) {
      WaitingCallbackContext waiting_callback =
          std::move(waiting_callbacks_.front());
      waiting_callbacks_.pop();

      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder =
          waiting_callback.oop_video_decoder
              ? std::move(waiting_callback.oop_video_decoder)
              : oop_video_decoder_.Unbind();

      if (waiting_callback.cb_task_runner->RunsTasksInCurrentSequence()) {
        // Release the lock in case the |waiting_callback|.cb wants to re-enter
        // OOPVideoDecoderSupportedConfigsManager by reaching
        // OOPVideoDecoderSupportedConfigsManager::Get() in the callback.
        base::AutoUnlock unlock(lock_);
        std::move(waiting_callback.cb).Run(std::move(oop_video_decoder));
      } else {
        waiting_callback.cb_task_runner->PostTask(
            FROM_HERE, base::BindOnce(std::move(waiting_callback.cb),
                                      std::move(oop_video_decoder)));
      }
    }
  }

  base::Lock lock_;

  // The first PendingRemote that NotifySupportKnown() is called with is bound
  // to |oop_video_decoder_| and we use it to query the supported configurations
  // of the out-of-process video decoder. |oop_video_decoder_| will get unbound
  // once the supported configurations are known.
  mojo::Remote<stable::mojom::StableVideoDecoder> oop_video_decoder_;

  // The cached supported video decoder configurations and decoder type.
  absl::optional<SupportedVideoDecoderConfigs> configs_ GUARDED_BY(lock_);
  absl::optional<VideoDecoderType> decoder_type_ GUARDED_BY(lock_);

  // This tracks everything that's needed to call a callback passed to
  // NotifySupportKnown() that had to be queued because there was a query in
  // progress.
  struct WaitingCallbackContext {
    WaitingCallbackContext(
        mojo::PendingRemote<stable::mojom::StableVideoDecoder>
            oop_video_decoder,
        base::OnceCallback<
            void(mojo::PendingRemote<stable::mojom::StableVideoDecoder>)> cb,
        scoped_refptr<base::SequencedTaskRunner> cb_task_runner)
        : oop_video_decoder(std::move(oop_video_decoder)),
          cb(std::move(cb)),
          cb_task_runner(std::move(cb_task_runner)) {}
    mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder;
    base::OnceCallback<void(
        mojo::PendingRemote<stable::mojom::StableVideoDecoder>)>
        cb;
    scoped_refptr<base::SequencedTaskRunner> cb_task_runner;
  };
  base::queue<WaitingCallbackContext> waiting_callbacks_ GUARDED_BY(lock_);
};

}  // namespace

// static
std::unique_ptr<VideoDecoderMixin> OOPVideoDecoder::Create(
    mojo::PendingRemote<stable::mojom::StableVideoDecoder>
        pending_remote_decoder,
    std::unique_ptr<media::MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client) {
  // TODO(b/171813538): make the destructor of this class (as well as the
  // destructor of sister class VaapiVideoDecoder) public so the explicit
  // argument can be removed from this call to base::WrapUnique().
  return base::WrapUnique<VideoDecoderMixin>(new OOPVideoDecoder(
      std::move(media_log), std::move(decoder_task_runner), std::move(client),
      std::move(pending_remote_decoder)));
}

// static
void OOPVideoDecoder::NotifySupportKnown(
    mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder,
    base::OnceCallback<
        void(mojo::PendingRemote<stable::mojom::StableVideoDecoder>)> cb) {
  OOPVideoDecoderSupportedConfigsManager::Instance().NotifySupportKnown(
      std::move(oop_video_decoder), std::move(cb));
}

// static
absl::optional<SupportedVideoDecoderConfigs>
OOPVideoDecoder::GetSupportedConfigs() {
  return OOPVideoDecoderSupportedConfigsManager::Instance().Get();
}

OOPVideoDecoder::OOPVideoDecoder(
    std::unique_ptr<media::MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client,
    mojo::PendingRemote<stable::mojom::StableVideoDecoder>
        pending_remote_decoder)
    : VideoDecoderMixin(std::move(media_log),
                        std::move(decoder_task_runner),
                        std::move(client)),
      fake_timestamp_to_real_timestamp_cache_(kTimestampCacheSize),
      remote_decoder_(std::move(pending_remote_decoder)),
      weak_this_factory_(this) {
  VLOGF(2);
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Set a connection error handler in case the remote decoder gets
  // disconnected, for instance, if the remote decoder process crashes.
  // The remote decoder lives in a utility process (for lacros-chrome,
  // this utility process is in ash-chrome).
  // base::Unretained() is safe because `this` owns the `mojo::Remote`.
  remote_decoder_.set_disconnect_handler(
      base::BindOnce(&OOPVideoDecoder::Stop, base::Unretained(this)));

  // TODO(b/195769334): |remote_consumer_handle| corresponds to the data pipe
  // that allows us to send data to the out-of-process video decoder. This data
  // pipe is separate from the one set up by renderers to send data to the GPU
  // process. Therefore, we're introducing the need for copying the encoded data
  // from one pipe to the other. Ideally, we would just forward the pipe
  // endpoint directly to the out-of-process video decoder and avoid the extra
  // copy. This would require us to plumb the mojo::ScopedDataPipeConsumerHandle
  // from the MojoVideoDecoderService all the way here.
  mojo::ScopedDataPipeConsumerHandle remote_consumer_handle;
  mojo_decoder_buffer_writer_ = MojoDecoderBufferWriter::Create(
      GetDefaultDecoderBufferConverterCapacity(DemuxerStream::VIDEO),
      &remote_consumer_handle);
  CHECK(mojo_decoder_buffer_writer_);

  DCHECK(!stable_video_frame_handle_releaser_remote_.is_bound());
  mojo::PendingReceiver<stable::mojom::VideoFrameHandleReleaser>
      stable_video_frame_handle_releaser_receiver =
          stable_video_frame_handle_releaser_remote_
              .BindNewPipeAndPassReceiver();

  // base::Unretained() is safe because `this` owns the `mojo::Remote`.
  stable_video_frame_handle_releaser_remote_.set_disconnect_handler(
      base::BindOnce(&OOPVideoDecoder::Stop, base::Unretained(this)));

  DCHECK(!stable_media_log_receiver_.is_bound());

  CHECK(!has_error_);
  // TODO(b/171813538): plumb the remaining parameters.
  remote_decoder_->Construct(
      client_receiver_.BindNewEndpointAndPassRemote(),
      stable_media_log_receiver_.BindNewPipeAndPassRemote(),
      std::move(stable_video_frame_handle_releaser_receiver),
      std::move(remote_consumer_handle), gfx::ColorSpace());
}

OOPVideoDecoder::~OOPVideoDecoder() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& pending_decode : pending_decodes_) {
    decoder_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(pending_decode.second),
                                  DecoderStatus::Codes::kAborted));
  }
}

void OOPVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                 bool low_delay,
                                 CdmContext* cdm_context,
                                 InitCB init_cb,
                                 const OutputCB& output_cb,
                                 const WaitingCB& waiting_cb) {
  DVLOGF(2) << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!init_cb_);
  CHECK(!HasPendingDecodeCallbacks());
  CHECK(!reset_cb_);

  // According to the VideoDecoder interface, Initialize() shouldn't be called
  // during pending decodes. Therefore, in addition to CHECK()ing that there are
  // no pending decode callbacks above, we also clear
  // |fake_timestamp_to_real_timestamp_cache_| which, together with the
  // validation in OnVideoFrameDecoded(), should guarantee that all frames
  // received going forward come from Decode() requests after this point.
  fake_timestamp_to_real_timestamp_cache_.Clear();

  if (has_error_) {
    // TODO(b/171813538): create specific error code for this decoder.
    std::move(init_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  mojo::PendingRemote<stable::mojom::StableCdmContext>
      pending_remote_stable_cdm_context;
  if (config.is_encrypted()) {
#if BUILDFLAG(IS_CHROMEOS)
    // There's logic in MojoVideoDecoderService::Initialize() to ensure that the
    // CDM doesn't change across Initialize() calls. We rely on this assumption
    // to ensure that creating a single StableCdmContextImpl that survives
    // re-initializations is correct: the remote decoder requires a bound
    // |pending_remote_stable_cdm_context| only for the first Initialize() call
    // that sets up encryption.
    DCHECK(!stable_cdm_context_ ||
           cdm_context == stable_cdm_context_->cdm_context());
    if (!stable_cdm_context_) {
      if (!cdm_context || !cdm_context->GetChromeOsCdmContext()) {
        std::move(init_cb).Run(
            DecoderStatus::Codes::kUnsupportedEncryptionMode);
        return;
      }
      stable_cdm_context_ =
          std::make_unique<chromeos::StableCdmContextImpl>(cdm_context);
      stable_cdm_context_receiver_ =
          std::make_unique<mojo::Receiver<stable::mojom::StableCdmContext>>(
              stable_cdm_context_.get(), pending_remote_stable_cdm_context
                                             .InitWithNewPipeAndPassReceiver());

      // base::Unretained() is safe because |this| owns the mojo::Receiver.
      stable_cdm_context_receiver_->set_disconnect_handler(
          base::BindOnce(&OOPVideoDecoder::Stop, base::Unretained(this)));
#if BUILDFLAG(USE_VAAPI)
      // We need to signal that for AMD we will do transcryption on the GPU
      // side. Then on the other end we just make transcryption a no-op.
      needs_transcryption_ = (VaapiWrapper::GetImplementationType() ==
                              VAImplementation::kMesaGallium);
#endif  // BUILDFLAG(USE_VAAPI)
    }
#else
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  initialized_for_protected_content_ = config.is_encrypted();

  init_cb_ = std::move(init_cb);
  output_cb_ = output_cb;
  waiting_cb_ = waiting_cb;

  remote_decoder_->Initialize(config, low_delay,
                              std::move(pending_remote_stable_cdm_context),
                              base::BindOnce(&OOPVideoDecoder::OnInitializeDone,
                                             weak_this_factory_.GetWeakPtr()));
}

void OOPVideoDecoder::OnInitializeDone(const DecoderStatus& status,
                                       bool needs_bitstream_conversion,
                                       int32_t max_decode_requests,
                                       VideoDecoderType decoder_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!has_error_);

  const VideoDecoderType expected_decoder_type =
      OOPVideoDecoderSupportedConfigsManager::Instance().GetDecoderType();

  if (!status.is_ok() || decoder_type != expected_decoder_type ||
      (remote_decoder_type_ != VideoDecoderType::kUnknown &&
       remote_decoder_type_ != decoder_type)) {
    Stop();
    return;
  }
  remote_decoder_type_ = decoder_type;
  std::move(init_cb_).Run(status);
}

void OOPVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                             DecodeCB decode_cb) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!init_cb_);
  CHECK(!reset_cb_);
  CHECK(!is_flushing_);

  if (has_error_ || remote_decoder_type_ == VideoDecoderType::kUnknown) {
    DeferDecodeCallback(std::move(decode_cb),
                        DecoderStatus::Codes::kNotInitialized);
    return;
  }

  if (decode_counter_ == std::numeric_limits<uint64_t>::max()) {
    // Error out in case of overflow.
    DeferDecodeCallback(std::move(decode_cb), DecoderStatus::Codes::kFailed);
    return;
  }

  CHECK(buffer);
  if (!buffer->end_of_stream()) {
    const base::TimeDelta next_fake_timestamp =
        current_fake_timestamp_ + base::Microseconds(1u);
    if (next_fake_timestamp == current_fake_timestamp_) {
      // We've reached the maximum base::TimeDelta.
      DeferDecodeCallback(std::move(decode_cb), DecoderStatus::Codes::kFailed);
      return;
    }
    current_fake_timestamp_ = next_fake_timestamp;
    DCHECK(
        fake_timestamp_to_real_timestamp_cache_.Peek(current_fake_timestamp_) ==
        fake_timestamp_to_real_timestamp_cache_.end());
    fake_timestamp_to_real_timestamp_cache_.Put(current_fake_timestamp_,
                                                buffer->timestamp());
    buffer->set_timestamp(current_fake_timestamp_);
  }

  const uint64_t decode_id = decode_counter_++;
  pending_decodes_.insert({decode_id, std::move(decode_cb)});

  mojom::DecoderBufferPtr mojo_buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(buffer);
  if (!mojo_buffer) {
    Stop();
    return;
  }

  is_flushing_ = buffer->end_of_stream();
  remote_decoder_->Decode(
      std::move(buffer),
      base::BindOnce(&OOPVideoDecoder::OnDecodeDone,
                     weak_this_factory_.GetWeakPtr(), decode_id, is_flushing_));
}

void OOPVideoDecoder::OnDecodeDone(uint64_t decode_id,
                                   bool is_flush_cb,
                                   const DecoderStatus& status) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!has_error_);

  // Check that decode callbacks are called in the same order as Decode().
  CHECK(!pending_decodes_.empty());
  if (pending_decodes_.cbegin()->first != decode_id) {
    VLOGF(2) << "Unexpected decode callback for request " << decode_id;
    Stop();
    return;
  }

  if (is_flush_cb) {
    CHECK(is_flushing_);

    // Check that the |decode_cb| corresponding to the flush is not called until
    // the decode callback has been called for each pending decode.
    CHECK_EQ(num_deferred_decode_cbs_, 0u);
    if (pending_decodes_.size() != 1) {
      VLOGF(2) << "Received a flush callback while having pending decodes";
      Stop();
      return;
    }

    // After a flush is completed, we shouldn't receive decoded frames
    // corresponding to Decode() calls that came in prior to the flush. The
    // clearing of the cache together with the validation in
    // OnVideoFrameDecoded() should guarantee this.
    fake_timestamp_to_real_timestamp_cache_.Clear();

    is_flushing_ = false;
  }

  auto it = pending_decodes_.begin();
  DecodeCB decode_cb = std::move(it->second);
  pending_decodes_.erase(it);
  std::move(decode_cb).Run(status);
}

void OOPVideoDecoder::DeferDecodeCallback(DecodeCB decode_cb,
                                          const DecoderStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/220915557): it's very unlikely that we'll get an integer overflow
  // here, but should we handle it gracefully if we do?
  CHECK_LT(num_deferred_decode_cbs_, std::numeric_limits<uint64_t>::max());
  num_deferred_decode_cbs_++;
  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OOPVideoDecoder::CallDeferredDecodeCallback,
                                weak_this_factory_.GetWeakPtr(),
                                std::move(decode_cb), status));
}

void OOPVideoDecoder::CallDeferredDecodeCallback(DecodeCB decode_cb,
                                                 const DecoderStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(decode_cb).Run(status);
  num_deferred_decode_cbs_--;
}

bool OOPVideoDecoder::HasPendingDecodeCallbacks() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !pending_decodes_.empty() || num_deferred_decode_cbs_ > 0;
}

void OOPVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!init_cb_);
  CHECK(!reset_cb_);

  reset_cb_ = std::move(reset_cb);

  if (has_error_ || remote_decoder_type_ == VideoDecoderType::kUnknown) {
    // Post a task instead of calling |reset_cb| immediately in order to keep
    // the relative order between decode callbacks (posted as tasks in Decode())
    // and the reset callback.
    //
    // Note: we don't post std::move(reset_cb_) as the task because we want
    // |reset_cb_| to be valid until it's actually called so that we can
    // properly enforce the VideoDecoder API requirement that no VideoDecoder
    // calls are made before the reset callback is executed.
    decoder_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&OOPVideoDecoder::CallResetCallback,
                                  weak_this_factory_.GetWeakPtr()));
    return;
  }

  remote_decoder_->Reset(base::BindOnce(&OOPVideoDecoder::OnResetDone,
                                        weak_this_factory_.GetWeakPtr()));
}

void OOPVideoDecoder::OnResetDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!has_error_);
  CHECK(reset_cb_);
  CHECK_EQ(num_deferred_decode_cbs_, 0u);
  if (!pending_decodes_.empty()) {
    VLOGF(2) << "Received a reset callback while having pending decodes";
    Stop();
    return;
  }

  // After a reset is completed, we shouldn't receive decoded frames
  // corresponding to Decode() calls that came in prior to the reset (similar to
  // a flush). That's because according to the media::VideoDecoder and
  // media::stable::mojom::StableVideoDecoder interfaces, all ongoing Decode()
  // requests must be completed or aborted prior to executing the reset
  // callback. The clearing of the cache together with the validation in
  // OnVideoFrameDecoded() should guarantee this.
  fake_timestamp_to_real_timestamp_cache_.Clear();

  CallResetCallback();
}

void OOPVideoDecoder::CallResetCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(reset_cb_);
  std::move(reset_cb_).Run();
}

void OOPVideoDecoder::Stop() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (has_error_)
    return;

  has_error_ = true;

  // There may be in-flight decode, initialize or reset callbacks.
  // Invalidate any outstanding weak pointers so those callbacks are ignored.
  weak_this_factory_.InvalidateWeakPtrs();

  // |init_cb_| is likely to reentrantly destruct |this|, so we check for that
  // using an on-stack WeakPtr.
  base::WeakPtr<OOPVideoDecoder> weak_this = weak_this_factory_.GetWeakPtr();

  client_receiver_.reset();
  stable_media_log_receiver_.reset();
  remote_decoder_.reset();
  mojo_decoder_buffer_writer_.reset();
  stable_video_frame_handle_releaser_remote_.reset();
  fake_timestamp_to_real_timestamp_cache_.Clear();

#if BUILDFLAG(IS_CHROMEOS)
  stable_cdm_context_receiver_.reset();
  stable_cdm_context_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (init_cb_)
    std::move(init_cb_).Run(DecoderStatus::Codes::kFailed);

  if (!weak_this)
    return;

  for (auto& pending_decode : pending_decodes_) {
    // Note that Stop() may be called from within Decode(), and according to the
    // media::VideoDecoder interface, the decode callback should not be called
    // from within Decode(). Therefore, we should not call the decode callbacks
    // here, and instead, we should post them as tasks.
    DeferDecodeCallback(std::move(pending_decode.second),
                        DecoderStatus::Codes::kFailed);
  }
  pending_decodes_.clear();
  is_flushing_ = false;

  if (reset_cb_) {
    // We post a task instead of calling |reset_cb_| immediately so that we keep
    // the order of pending decode callbacks (posted as tasks above) with
    // respect to the reset callback.
    decoder_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&OOPVideoDecoder::CallResetCallback,
                                  weak_this_factory_.GetWeakPtr()));
  }
}

void OOPVideoDecoder::ReleaseVideoFrame(
    const base::UnguessableToken& release_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!has_error_);
  CHECK(stable_video_frame_handle_releaser_remote_.is_bound());

  stable_video_frame_handle_releaser_remote_->ReleaseVideoFrame(release_token);
}

void OOPVideoDecoder::ApplyResolutionChange() {
  NOTREACHED();
}

bool OOPVideoDecoder::NeedsBitstreamConversion() const {
  NOTIMPLEMENTED();
  NOTREACHED();
  return false;
}

bool OOPVideoDecoder::CanReadWithoutStalling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!init_cb_);
  // TODO(b/220915557): according to the VideoDecoder interface, no VideoDecoder
  // calls should be made before the reset callback is executed. In theory, this
  // includes CanReadWithoutStalling(). However, asserting this through the
  // commented CHECK(!reset_cb_) below causes a crash because we need to call
  // CanReadWithoutStalling() in the frame output callback
  // (VideoDecoderPipeline::OnFrameDecoded()) which can happen in an in-progress
  // Reset(). It's likely that the VideoDecoder restriction expressed above does
  // not include CanReadWithoutStalling() because
  // MojoVideoDecoderService::OnDecoderOutput() (a frame output callback)
  // already calls VideoDecoder::CanReadWithoutStalling(). If so, then we should
  // update the VideoDecoder::Reset() documentation.
  // CHECK(!reset_cb_);
  CHECK(!has_error_);
  return can_read_without_stalling_;
}

int OOPVideoDecoder::GetMaxDecodeRequests() const {
  NOTIMPLEMENTED();
  NOTREACHED();
  return 4;
}

VideoDecoderType OOPVideoDecoder::GetDecoderType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!init_cb_);
  CHECK(!reset_cb_);
  return VideoDecoderType::kOutOfProcess;
}

bool OOPVideoDecoder::IsPlatformDecoder() const {
  NOTIMPLEMENTED();
  NOTREACHED();
  return true;
}

bool OOPVideoDecoder::NeedsTranscryption() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return needs_transcryption_;
}

void OOPVideoDecoder::OnVideoFrameDecoded(
    stable::mojom::VideoFramePtr frame,
    bool can_read_without_stalling,
    const base::UnguessableToken& release_token) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!has_error_);

  if (init_cb_) {
    VLOGF(2) << "Received a decoded frame while waiting for initialization";
    Stop();
    return;
  }

  // According to the media::VideoDecoder API, |output_cb_| should not be
  // supplied with EOS frames. The mojo traits guarantee this DCHECK.
  DCHECK(!frame->metadata.end_of_stream);

  if (!gfx::Rect(frame->coded_size).Contains(frame->visible_rect)) {
    VLOGF(2) << "Received a frame with inconsistent coded size and visible "
                "rectangle";
    Stop();
    return;
  }

  const base::TimeDelta fake_timestamp = frame->timestamp;
  auto it = fake_timestamp_to_real_timestamp_cache_.Get(fake_timestamp);
  if (it == fake_timestamp_to_real_timestamp_cache_.end()) {
    // The remote decoder is misbehaving.
    VLOGF(2) << "Received an unexpected decoded frame";
    Stop();
    return;
  }
  const base::TimeDelta real_timestamp = it->second;

  // Validate protected content metadata.
  if (!initialized_for_protected_content_ &&
      (frame->metadata.protected_video || frame->metadata.hw_protected)) {
    VLOGF(2) << "Received a frame with unexpected metadata from a decoder that "
                "was not configured for protected content";
    Stop();
    return;
  }
  if (initialized_for_protected_content_ &&
      (!frame->metadata.protected_video || !frame->metadata.hw_protected)) {
    VLOGF(2) << "Received a frame with unexpected metadata from a decoder that "
                "was configured for protected content";
    Stop();
    return;
  }

  scoped_refptr<VideoFrame> gmb_frame =
      MojoVideoFrameToMediaVideoFrame(std::move(frame));
  if (!gmb_frame) {
    Stop();
    return;
  }
  gmb_frame->set_timestamp(real_timestamp);

  // The destruction observer will be called after the client releases the
  // video frame. base::BindPostTaskToCurrentDefault() is used to make sure that
  // the WeakPtr is dereferenced on the correct sequence.
  gmb_frame->AddDestructionObserver(base::BindPostTaskToCurrentDefault(
      base::BindOnce(&OOPVideoDecoder::ReleaseVideoFrame,
                     weak_this_factory_.GetWeakPtr(), release_token)));

  can_read_without_stalling_ = can_read_without_stalling;

  if (output_cb_)
    output_cb_.Run(std::move(gmb_frame));
}

void OOPVideoDecoder::OnWaiting(WaitingReason reason) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!has_error_);

  // Note: the remote video decoder may be of a newer version than us (see e.g.,
  // go/lacros-version-skew-guide). Therefore, we may get the default
  // WaitingReason::kNoCdm if the value received over mojo is unrecognized. It's
  // not expected that we'll ever use WaitingReason::kNoCdm for anything
  // legitimate in ChromeOS, so if we receive that for any reason, the remote
  // decoder is either misbehaving or too new.
  if (reason == WaitingReason::kNoCdm) {
    VLOGF(2) << "Received an unexpected WaitingReason";
    Stop();
    return;
  }

  if (waiting_cb_)
    waiting_cb_.Run(reason);
}

void OOPVideoDecoder::AddLogRecord(const MediaLogRecord& event) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(b/220915557): we should validate |event| before using it since we
  // can't trust anything coming from the remote decoder.
  // if (media_log_)
  //   media_log_->AddLogRecord(std::make_unique<media::MediaLogRecord>(event));
}

}  // namespace media
