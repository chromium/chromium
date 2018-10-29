// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/android_video_decode_accelerator.h"

#include <stddef.h>

#include <memory>

#include "base/android/build_info.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/base/android/media_codec_bridge_impl.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/limits.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/timestamp_constants.h"
#include "media/base/unaligned_shared_memory.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/android/android_video_surface_chooser_impl.h"
#include "media/gpu/android/avda_picture_buffer_manager.h"
#include "media/gpu/android/device_info.h"
#include "media/gpu/android/promotion_hint_aggregator_impl.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/video/picture.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#include "media/cdm/cdm_manager.h"  // nogncheck
#endif

#define NOTIFY_ERROR(error_code, error_message)      \
  do {                                               \
    DLOG(ERROR) << error_message;                    \
    NotifyError(VideoDecodeAccelerator::error_code); \
  } while (0)

namespace media {

namespace {

enum { kNumPictureBuffers = limits::kMaxVideoFrames + 1 };

// Max number of bitstreams notified to the client with
// NotifyEndOfBitstreamBuffer() before getting output from the bitstream.
enum { kMaxBitstreamsNotifiedInAdvance = 32 };

// MediaCodec is only guaranteed to support baseline, but some devices may
// support others. Advertise support for all H264 profiles and let the
// MediaCodec fail when decoding if it's not actually supported. It's assumed
// that consumers won't have software fallback for H264 on Android anyway.
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
constexpr VideoCodecProfile kSupportedH264Profiles[] = {
    H264PROFILE_BASELINE,
    H264PROFILE_MAIN,
    H264PROFILE_EXTENDED,
    H264PROFILE_HIGH,
    H264PROFILE_HIGH10PROFILE,
    H264PROFILE_HIGH422PROFILE,
    H264PROFILE_HIGH444PREDICTIVEPROFILE,
    H264PROFILE_SCALABLEBASELINE,
    H264PROFILE_SCALABLEHIGH,
    H264PROFILE_STEREOHIGH,
    H264PROFILE_MULTIVIEWHIGH};

#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
constexpr VideoCodecProfile kSupportedHevcProfiles[] = {HEVCPROFILE_MAIN,
                                                        HEVCPROFILE_MAIN10};
#endif
#endif

// Because MediaCodec is thread-hostile (must be poked on a single thread) and
// has no callback mechanism (b/11990118), we must drive it by polling for
// complete frames (and available input buffers, when the codec is fully
// saturated).  This function defines the polling delay.  The value used is an
// arbitrary choice that trades off CPU utilization (spinning) against latency.
// Mirrors android_video_encode_accelerator.cc:EncodePollDelay().
//
// An alternative to this polling scheme could be to dedicate a new thread
// (instead of using the ChildThread) to run the MediaCodec, and make that
// thread use the timeout-based flavor of MediaCodec's dequeue methods when it
// believes the codec should complete "soon" (e.g. waiting for an input
// buffer, or waiting for a picture when it knows enough complete input
// pictures have been fed to saturate any internal buffering).  This is
// speculative and it's unclear that this would be a win (nor that there's a
// reasonably device-agnostic way to fill in the "believes" above).
constexpr base::TimeDelta DecodePollDelay =
    base::TimeDelta::FromMilliseconds(10);

constexpr base::TimeDelta NoWaitTimeOut = base::TimeDelta::FromMicroseconds(0);

constexpr base::TimeDelta IdleTimerTimeOut = base::TimeDelta::FromSeconds(1);

// On low end devices (< KitKat is always low-end due to buggy MediaCodec),
// defer the surface creation until the codec is actually used if we know no
// software fallback exists.
bool ShouldDeferSurfaceCreation(CodecAllocator* codec_allocator,
                                const OverlayInfo& overlay_info,
                                VideoCodec codec,
                                DeviceInfo* device_info) {
  // TODO(liberato): We might still want to defer if we've got a routing
  // token.  It depends on whether we want to use it right away or not.
  if (overlay_info.HasValidRoutingToken())
    return false;

  return codec == kCodecH264 && codec_allocator->IsAnyRegisteredAVDA() &&
         device_info->SdkVersion() <= base::android::SDK_VERSION_JELLY_BEAN_MR2;
}

bool HasValidCdm(int cdm_id) {
#if !BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  return false;
#else
  auto cdm = CdmManager::GetInstance()->GetCdm(cdm_id);
  if (!cdm) {
    // This could happen during the destruction of the media element and the CDM
    // and due to IPC CDM could be destroyed before the decoder.
    DVLOG(1) << "CDM not available.";
    return false;
  }

  auto* cdm_context = cdm->GetCdmContext();
  auto* media_crypto_context =
      cdm_context ? cdm_context->GetMediaCryptoContext() : nullptr;
  // This could happen if the CDM is not MediaDrmBridge, which could happen in
  // test cases.
  if (!media_crypto_context) {
    DVLOG(1) << "MediaCryptoContext not available.";
    return false;
  }

  return true;
#endif
}

}  // namespace

// AVDAManager manages a RepeatingTimer so that AVDAs can get a regular callback
// to DoIOTask().
class AVDAManager {
 public:
  AVDAManager() {}

  // Request periodic callback of |avda|->DoIOTask(). Does nothing if the
  // instance is already registered and the timer started. The first request
  // will start the repeating timer on an interval of DecodePollDelay.
  void StartTimer(AndroidVideoDecodeAccelerator* avda) {
    DCHECK(thread_checker_.CalledOnValidThread());

    timer_avda_instances_.insert(avda);

    // If the timer is running, StopTimer() might have been called earlier, if
    // so remove the instance from the pending erasures.
    if (timer_running_)
      pending_erase_.erase(avda);

    if (io_timer_.IsRunning())
      return;
    io_timer_.Start(FROM_HERE, DecodePollDelay, this, &AVDAManager::RunTimer);
  }

  // Stop callbacks to |avda|->DoIOTask(). Does nothing if the instance is not
  // registered. If there are no instances left, the repeating timer will be
  // stopped.
  void StopTimer(AndroidVideoDecodeAccelerator* avda) {
    DCHECK(thread_checker_.CalledOnValidThread());

    // If the timer is running, defer erasures to avoid iterator invalidation.
    if (timer_running_) {
      pending_erase_.insert(avda);
      return;
    }

    timer_avda_instances_.erase(avda);
    if (timer_avda_instances_.empty())
      io_timer_.Stop();
  }

 private:
  ~AVDAManager() = delete;

  void RunTimer() {
    {
      // Call out to all AVDA instances, some of which may attempt to remove
      // themselves from the list during this operation; those removals will be
      // deferred until after all iterations are complete.
      base::AutoReset<bool> scoper(&timer_running_, true);
      for (auto* avda : timer_avda_instances_)
        avda->DoIOTask(false);
    }

    // Take care of any deferred erasures.
    for (auto* avda : pending_erase_)
      StopTimer(avda);
    pending_erase_.clear();

    // TODO(dalecurtis): We may want to consider chunking this if task execution
    // takes too long for the combined timer.
  }

  // All AVDA instances that would like us to poll DoIOTask.
  std::set<AndroidVideoDecodeAccelerator*> timer_avda_instances_;

  // Since we can't delete while iterating when using a set, defer erasure until
  // after iteration complete.
  bool timer_running_ = false;
  std::set<AndroidVideoDecodeAccelerator*> pending_erase_;

  // Repeating timer responsible for draining pending IO to the codecs.
  base::RepeatingTimer io_timer_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AVDAManager);
};

static AVDAManager* GetManager() {
  static AVDAManager* manager = new AVDAManager();
  return manager;
}

AndroidVideoDecodeAccelerator::BitstreamRecord::BitstreamRecord(
    const BitstreamBuffer& bitstream_buffer)
    : buffer(bitstream_buffer) {
  if (buffer.id() != -1) {
    memory.reset(
        new UnalignedSharedMemory(buffer.handle(), buffer.size(), true));
  }
}

AndroidVideoDecodeAccelerator::BitstreamRecord::BitstreamRecord(
    BitstreamRecord&& other)
    : buffer(std::move(other.buffer)), memory(std::move(other.memory)) {}

AndroidVideoDecodeAccelerator::BitstreamRecord::~BitstreamRecord() {}

AndroidVideoDecodeAccelerator::AndroidVideoDecodeAccelerator(
    CodecAllocator* codec_allocator,
    std::unique_ptr<AndroidVideoSurfaceChooser> surface_chooser,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const GetContextGroupCallback& get_context_group_cb,
    const AndroidOverlayMojoFactoryCB& overlay_factory_cb,
    DeviceInfo* device_info)
    : client_(nullptr),
      codec_allocator_(codec_allocator),
      make_context_current_cb_(make_context_current_cb),
      get_context_group_cb_(get_context_group_cb),
      state_(BEFORE_OVERLAY_INIT),
      picturebuffers_requested_(false),
      picture_buffer_manager_(this),
      media_crypto_context_(nullptr),
      cdm_registration_id_(0),
      pending_input_buf_index_(-1),
      during_initialize_(false),
      deferred_initialization_pending_(false),
      codec_needs_reset_(false),
      defer_surface_creation_(false),
      surface_chooser_helper_(
          std::move(surface_chooser),
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kForceVideoOverlays),
          base::FeatureList::IsEnabled(media::kUseAndroidOverlayAggressively),
          false /* always_use_texture_owner */),
      device_info_(device_info),
      force_defer_surface_creation_for_testing_(false),
      force_allow_software_decoding_for_testing_(false),
      overlay_factory_cb_(overlay_factory_cb),
      weak_this_factory_(this) {}

AndroidVideoDecodeAccelerator::~AndroidVideoDecodeAccelerator() {
  DCHECK(thread_checker_.CalledOnValidThread());
  GetManager()->StopTimer(this);
  codec_allocator_->StopThread(this);

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  if (!media_crypto_context_)
    return;

  // Cancel previously registered callback (if any).
  media_crypto_context_->SetMediaCryptoReadyCB(
      MediaCryptoContext::MediaCryptoReadyCB());

  if (cdm_registration_id_)
    media_crypto_context_->UnregisterPlayer(cdm_registration_id_);
#endif  // BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
}

bool AndroidVideoDecodeAccelerator::Initialize(const Config& config,
                                               Client* client) {
  DVLOG(1) << __func__ << ": " << config.AsHumanReadableString();
  TRACE_EVENT0("media", "AVDA::Initialize");
  DCHECK(!media_codec_);
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoReset<bool> scoper(&during_initialize_, true);

  if (!make_context_current_cb_ || !get_context_group_cb_) {
    DLOG(ERROR) << "GL callbacks are required for this VDA";
    return false;
  }

  if (config.output_mode != Config::OutputMode::ALLOCATE) {
    DLOG(ERROR) << "Only ALLOCATE OutputMode is supported by this VDA";
    return false;
  }

  DCHECK(client);
  client_ = client;
  config_ = config;
  codec_config_ = new CodecConfig();
  codec_config_->codec = VideoCodecProfileToVideoCodec(config.profile);
  codec_config_->initial_expected_coded_size =
      config.initial_expected_coded_size;

  switch (codec_config_->codec) {
    case kCodecVP8:
    case kCodecVP9:
      break;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case kCodecH264:
      codec_config_->csd0 = config.sps;
      codec_config_->csd1 = config.pps;
      break;
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
    case kCodecHEVC:
      break;
#endif
#endif
    default:
      DLOG(ERROR) << "Unsupported profile: " << GetProfileName(config.profile);
      return false;
  }

  codec_config_->software_codec_forbidden =
      IsMediaCodecSoftwareDecodingForbidden();

  codec_config_->container_color_space = config.container_color_space;
  codec_config_->hdr_metadata = config.hdr_metadata;

  // Only use MediaCodec for VP8/9 if it's likely backed by hardware
  // or if the stream is encrypted.
  if (IsMediaCodecSoftwareDecodingForbidden() &&
      MediaCodecUtil::IsKnownUnaccelerated(codec_config_->codec,
                                           MediaCodecDirection::DECODER)) {
    DVLOG(1) << "Initialization failed: " << GetCodecName(codec_config_->codec)
             << " is not hardware accelerated";
    return false;
  }

  auto* context_group = get_context_group_cb_.Run();
  if (!context_group) {
    DLOG(ERROR) << "Failed to get context group.";
    return false;
  }

  // We signaled that we support deferred initialization, so see if the client
  // does also.
  deferred_initialization_pending_ = config.is_deferred_initialization_allowed;

  // If we're low on resources, we may decide to defer creation of the surface
  // until the codec is actually used.
  if (force_defer_surface_creation_for_testing_ ||
      ShouldDeferSurfaceCreation(codec_allocator_, config_.overlay_info,
                                 codec_config_->codec, device_info_)) {
    // We should never be here if a SurfaceView is required.
    // TODO(liberato): This really isn't true with AndroidOverlay.
    defer_surface_creation_ = true;
  }

  codec_allocator_->StartThread(this);

  // If has valid CDM, start by initializing the CDM, even for clear stream.
  if (HasValidCdm(config_.cdm_id) && deferred_initialization_pending_) {
    InitializeCdm();
    return state_ != ERROR;
  }

  // Cannot handle encrypted stream without valid CDM.
  if (config_.is_encrypted()) {
    DLOG(ERROR) << "Deferred initialization must be used for encrypted streams";
    return false;
  }

  StartSurfaceChooser();

  // Fail / complete / defer initialization.
  return state_ != ERROR;
}

void AndroidVideoDecodeAccelerator::StartSurfaceChooser() {
  DCHECK_EQ(state_, BEFORE_OVERLAY_INIT);

  // If we're trying to defer surface creation, then don't notify the chooser
  // that it may start getting surfaces yet.  We'll do that later.
  if (defer_surface_creation_) {
    if (deferred_initialization_pending_)
      NotifyInitializationSucceeded();
    return;
  }

  surface_chooser_helper_.SetIsFullscreen(config_.overlay_info.is_fullscreen);

  surface_chooser_helper_.chooser()->SetClientCallbacks(
      base::Bind(&AndroidVideoDecodeAccelerator::OnSurfaceTransition,
                 weak_this_factory_.GetWeakPtr()),
      base::Bind(&AndroidVideoDecodeAccelerator::OnSurfaceTransition,
                 weak_this_factory_.GetWeakPtr(), nullptr));

  // Handle the sync path, which must use TextureOwner anyway.  Note that we
  // check both |during_initialize_| and |deferred_initialization_pending_|,
  // since we might get here during deferred surface creation.  In that case,
  // Decode will call us (after clearing |defer_surface_creation_|), but
  // deferred init will have already been signaled optimistically as success.
  //
  // Also note that we might choose to defer surface creation for the sync path,
  // which won't get here.  We'll exit above, successfully, during init, and
  // will fall through to the below when Decode calls us back.  That's okay.
  // We only handle this case specially since |surface_chooser_| is allowed to
  // post callbacks to us.  Here, we guarantee that the sync case is actually
  // resolved synchronously.  The only exception will be if we need to defer
  // surface creation for other reasons, in which case the sync path with just
  // signal success optimistically.
  if (during_initialize_ && !deferred_initialization_pending_) {
    DCHECK(!config_.overlay_info.HasValidRoutingToken());
    // Note that we might still send feedback to |surface_chooser_|, which might
    // call us back.  However, it will only ever tell us to use TextureOwner,
    // since we have no overlay factory anyway.
    OnSurfaceTransition(nullptr);
    return;
  }

  // If we have a surface, then notify |surface_chooser_| about it.  If we were
  // told not to use an overlay (kNoSurfaceID or a null routing token), then we
  // leave the factory blank.
  AndroidOverlayFactoryCB factory;
  if (config_.overlay_info.HasValidRoutingToken() && overlay_factory_cb_) {
    factory = base::BindRepeating(overlay_factory_cb_,
                                  *config_.overlay_info.routing_token);
  }

  // Notify |surface_chooser_| that we've started.  This guarantees that we'll
  // get a callback.  It might not be a synchronous callback, but we're not in
  // the synchronous case.  It will be soon, though.  For pre-M, we rely on the
  // fact that |surface_chooser_| won't tell us to use a TextureOwner while
  // waiting for an overlay to become ready, for example.
  surface_chooser_helper_.UpdateChooserState(std::move(factory));
}

void AndroidVideoDecodeAccelerator::OnSurfaceTransition(
    std::unique_ptr<AndroidOverlay> overlay) {
  if (overlay) {
    overlay->AddSurfaceDestroyedCallback(base::Bind(
        &AndroidVideoDecodeAccelerator::OnStopUsingOverlayImmediately,
        weak_this_factory_.GetWeakPtr()));
  }

  // If we're waiting for a surface (e.g., during startup), then proceed
  // immediately.  Otherwise, wait for Dequeue to handle it.  This can probably
  // be merged with UpdateSurface.
  if (state_ == BEFORE_OVERLAY_INIT) {
    DCHECK(!incoming_overlay_);
    incoming_bundle_ = new AVDASurfaceBundle(std::move(overlay));
    InitializePictureBufferManager();
    return;
  }

  // If, for some reason, |surface_chooser_| decides that we really should
  // change our output surface pre-M, ignore it.  For example, if the
  // compositor tells us that it can't use an overlay, well, there's not much
  // that we can do here unless we start falling forward to keyframes.
  if (!device_info_->IsSetOutputSurfaceSupported())
    return;

  // If we're using a TextureOwner and are told to switch to one, then just
  // do nothing.  |surface_chooser_| doesn't really know if we've switched to
  // TextureOwner or not.  Note that it can't ask us to switch to the same
  // overlay we're using, since it's unique_ptr.
  if (!overlay && codec_config_->surface_bundle &&
      !codec_config_->surface_bundle->overlay) {
    // Also stop transitioning to an overlay, if we were doing so.
    incoming_overlay_.reset();
    return;
  }

  incoming_overlay_ = std::move(overlay);
}

void AndroidVideoDecodeAccelerator::InitializePictureBufferManager() {
  DCHECK(!defer_surface_creation_);
  DCHECK(incoming_bundle_);

  if (!make_context_current_cb_.Run()) {
    NOTIFY_ERROR(PLATFORM_FAILURE,
                 "Failed to make this decoder's GL context current");
    incoming_bundle_ = nullptr;
    return;
  }

  // Move |incoming_bundle_| to |codec_config_|.  Our caller must set up an
  // incoming bundle properly, since we don't want to accidentally overwrite
  // |surface_bundle| for a codec that's being released elsewhere.
  // TODO(liberato): it doesn't make sense anymore for the PictureBufferManager
  // to create the texture owner.  We can probably make an overlay impl out
  // of it, and provide the texture owner to |picture_buffer_manager_|.
  if (!picture_buffer_manager_.Initialize(incoming_bundle_)) {
    NOTIFY_ERROR(PLATFORM_FAILURE, "Could not allocate texture owner");
    incoming_bundle_ = nullptr;
    return;
  }

  // If we have a media codec, then SetSurface.  If that doesn't work, then we
  // do not try to allocate a new codec; we might not be at a keyframe, etc.
  // If we get here with a codec, then we must setSurface.
  if (media_codec_) {
    // TODO(liberato): fail on api check?
    if (!media_codec_->SetSurface(incoming_bundle_->GetJavaSurface())) {
      NOTIFY_ERROR(PLATFORM_FAILURE, "MediaCodec failed to switch surfaces.");
      // We're not going to use |incoming_bundle_|.
    } else {
      // We've switched surfaces, so replace |surface_bundle|.
      codec_config_->surface_bundle = incoming_bundle_;
      // We could be in BEFORE_OVERLAY_INIT, but we're not anymore.
      state_ = NO_ERROR;
    }
    incoming_bundle_ = nullptr;
    CacheFrameInformation();
    return;
  }

  // We're going to create a codec with |incoming_bundle_|.  It might fail, but
  // either way, we're done with any previous bundle.  Note that, since we
  // never get here after init (i.e., we never change surfaces without using
  // SetSurface), there shouldn't be any previous bundle.  However, this is the
  // right thing to do even if we can switch.
  codec_config_->surface_bundle = incoming_bundle_;
  incoming_bundle_ = nullptr;
  CacheFrameInformation();

  // If the client doesn't support deferred initialization (WebRTC), then we
  // should complete it now and return a meaningful result.  Note that it would
  // be nice if we didn't have to worry about starting codec configuration at
  // all (::Initialize or the wrapper can do it), but then they have to remember
  // not to start codec config if we have to wait for the cdm.  It's somewhat
  // clearer for us to handle both cases.
  // For this to be a case for sync configuration, we must be called from
  // Initialize(), and the client must not want deferred init.  Note that having
  // |deferred_initialization_pending_| false by itself isn't enough; if we're
  // deferring surface creation, then we'll finish deferred init before asking
  // for the surface.  We'll be called via Decode.
  if (during_initialize_ && !deferred_initialization_pending_) {
    ConfigureMediaCodecSynchronously();
    return;
  }

  // In all other cases, we don't have to wait for the codec.
  ConfigureMediaCodecAsynchronously();
}

void AndroidVideoDecodeAccelerator::DoIOTask(bool start_timer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("media", "AVDA::DoIOTask");
  if (state_ == ERROR || state_ == WAITING_FOR_CODEC ||
      state_ == SURFACE_DESTROYED || state_ == BEFORE_OVERLAY_INIT) {
    return;
  }

  picture_buffer_manager_.MaybeRenderEarly();
  bool did_work = false, did_input = false, did_output = false;
  do {
    did_input = QueueInput();
    did_output = DequeueOutput();
    if (did_input || did_output)
      did_work = true;
  } while (did_input || did_output);

  ManageTimer(did_work || start_timer);
}

bool AndroidVideoDecodeAccelerator::QueueInput() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("media", "AVDA::QueueInput");
  if (state_ == ERROR || state_ == WAITING_FOR_CODEC ||
      state_ == WAITING_FOR_KEY || state_ == BEFORE_OVERLAY_INIT) {
    return false;
  }
  if (bitstreams_notified_in_advance_.size() > kMaxBitstreamsNotifiedInAdvance)
    return false;
  if (pending_bitstream_records_.empty())
    return false;

  int input_buf_index = pending_input_buf_index_;

  // Do not dequeue a new input buffer if we failed with MEDIA_CODEC_NO_KEY.
  // That status does not return this buffer back to the pool of
  // available input buffers. We have to reuse it in QueueSecureInputBuffer().
  if (input_buf_index == -1) {
    MediaCodecStatus status =
        media_codec_->DequeueInputBuffer(NoWaitTimeOut, &input_buf_index);
    switch (status) {
      case MEDIA_CODEC_TRY_AGAIN_LATER:
        return false;
      case MEDIA_CODEC_ERROR:
        NOTIFY_ERROR(PLATFORM_FAILURE, "DequeueInputBuffer failed");
        return false;
      case MEDIA_CODEC_OK:
        break;
      default:
        NOTREACHED();
        return false;
    }
  }

  DCHECK_NE(input_buf_index, -1);

  BitstreamBuffer bitstream_buffer = pending_bitstream_records_.front().buffer;

  if (bitstream_buffer.id() == -1) {
    pending_bitstream_records_.pop();
    TRACE_COUNTER1("media", "AVDA::PendingBitstreamBufferCount",
                   pending_bitstream_records_.size());

    media_codec_->QueueEOS(input_buf_index);
    return true;
  }

  std::unique_ptr<UnalignedSharedMemory> shm;

  if (pending_input_buf_index_ == -1) {
    // When |pending_input_buf_index_| is not -1, the buffer is already dequeued
    // from MediaCodec, filled with data and bitstream_buffer.handle() is
    // closed.
    shm = std::move(pending_bitstream_records_.front().memory);
    auto* buffer = &pending_bitstream_records_.front().buffer;

    if (!shm->MapAt(buffer->offset(), buffer->size())) {
      NOTIFY_ERROR(UNREADABLE_INPUT, "UnalignedSharedMemory::Map() failed");
      return false;
    }
  }

  const base::TimeDelta presentation_timestamp =
      bitstream_buffer.presentation_timestamp();
  DCHECK(presentation_timestamp != kNoTimestamp)
      << "Bitstream buffers must have valid presentation timestamps";

  // There may already be a bitstream buffer with this timestamp, e.g., VP9 alt
  // ref frames, but it's OK to overwrite it because we only expect a single
  // output frame to have that timestamp. AVDA clients only use the bitstream
  // buffer id in the returned Pictures to map a bitstream buffer back to a
  // timestamp on their side, so either one of the bitstream buffer ids will
  // result in them finding the right timestamp.
  bitstream_buffers_in_decoder_[presentation_timestamp] = bitstream_buffer.id();

  // Notice that |memory| will be null if we repeatedly enqueue the same buffer,
  // this happens after MEDIA_CODEC_NO_KEY.
  const uint8_t* memory =
      shm ? static_cast<const uint8_t*>(shm->memory()) : nullptr;
  const std::string& key_id = bitstream_buffer.key_id();
  const std::string& iv = bitstream_buffer.iv();
  const std::vector<SubsampleEntry>& subsamples = bitstream_buffer.subsamples();

  MediaCodecStatus status;
  if (key_id.empty() || iv.empty()) {
    status = media_codec_->QueueInputBuffer(input_buf_index, memory,
                                            bitstream_buffer.size(),
                                            presentation_timestamp);
  } else {
    // VDAs only support "cenc" encryption scheme.
    status = media_codec_->QueueSecureInputBuffer(
        input_buf_index, memory, bitstream_buffer.size(), key_id, iv,
        subsamples, AesCtrEncryptionScheme(), presentation_timestamp);
  }

  DVLOG(2) << __func__
           << ": Queue(Secure)InputBuffer: pts:" << presentation_timestamp
           << " status:" << status;

  if (status == MEDIA_CODEC_NO_KEY) {
    // Keep trying to enqueue the same input buffer.
    // The buffer is owned by us (not the MediaCodec) and is filled with data.
    DVLOG(1) << "QueueSecureInputBuffer failed: NO_KEY";
    pending_input_buf_index_ = input_buf_index;
    state_ = WAITING_FOR_KEY;
    return false;
  }

  pending_input_buf_index_ = -1;
  pending_bitstream_records_.pop();
  TRACE_COUNTER1("media", "AVDA::PendingBitstreamBufferCount",
                 pending_bitstream_records_.size());
  // We should call NotifyEndOfBitstreamBuffer(), when no more decoded output
  // will be returned from the bitstream buffer. However, MediaCodec API is
  // not enough to guarantee it.
  // So, here, we calls NotifyEndOfBitstreamBuffer() in advance in order to
  // keep getting more bitstreams from the client, and throttle them by using
  // |bitstreams_notified_in_advance_|.
  // TODO(dwkang): check if there is a way to remove this workaround.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer,
                 weak_this_factory_.GetWeakPtr(), bitstream_buffer.id()));
  bitstreams_notified_in_advance_.push_back(bitstream_buffer.id());

  if (status != MEDIA_CODEC_OK) {
    NOTIFY_ERROR(PLATFORM_FAILURE, "QueueInputBuffer failed:" << status);
    return false;
  }

  return true;
}

bool AndroidVideoDecodeAccelerator::DequeueOutput() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("media", "AVDA::DequeueOutput");
  if (state_ == ERROR || state_ == WAITING_FOR_CODEC ||
      state_ == BEFORE_OVERLAY_INIT) {
    return false;
  }
  // If we're draining for reset or destroy, then we don't need picture buffers
  // since we won't send any decoded frames anyway.  There might not be any,
  // since the pipeline might not be sending them back and / or they don't
  // exist anymore.  From the pipeline's point of view, for Destroy at least,
  // the VDA is already gone.
  if (picturebuffers_requested_ && output_picture_buffers_.empty() &&
      !IsDrainingForResetOrDestroy()) {
    return false;
  }
  if (!output_picture_buffers_.empty() && free_picture_ids_.empty() &&
      !IsDrainingForResetOrDestroy()) {
    // Don't have any picture buffer to send. Need to wait.
    return false;
  }

  // If we're waiting to switch surfaces pause output release until we have all
  // picture buffers returned. This is so we can ensure the right flags are set
  // on the picture buffers returned to the client.
  if (incoming_overlay_) {
    if (picture_buffer_manager_.HasUnrenderedPictures())
      return false;
    if (!UpdateSurface())
      return false;

    // UpdateSurface should fail if we've transitioned to the error state.
    DCHECK(state_ == NO_ERROR);
  }

  bool eos = false;
  base::TimeDelta presentation_timestamp;
  int32_t buf_index = 0;
  do {
    size_t offset = 0;
    size_t size = 0;

    TRACE_EVENT_BEGIN0("media", "AVDA::DequeueOutput");
    MediaCodecStatus status = media_codec_->DequeueOutputBuffer(
        NoWaitTimeOut, &buf_index, &offset, &size, &presentation_timestamp,
        &eos, NULL);
    TRACE_EVENT_END2("media", "AVDA::DequeueOutput", "status", status,
                     "presentation_timestamp (ms)",
                     presentation_timestamp.InMilliseconds());

    switch (status) {
      case MEDIA_CODEC_ERROR:
        // Do not post an error if we are draining for reset and destroy.
        // Instead, signal completion of the drain.
        if (IsDrainingForResetOrDestroy()) {
          DVLOG(1) << __func__ << ": error while draining";
          state_ = ERROR;
          OnDrainCompleted();
        } else {
          NOTIFY_ERROR(PLATFORM_FAILURE, "DequeueOutputBuffer failed.");
        }
        return false;

      case MEDIA_CODEC_TRY_AGAIN_LATER:
        return false;

      case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED: {
        // An OUTPUT_FORMAT_CHANGED is not reported after flush() if the frame
        // size does not change. Therefore we have to keep track on the format
        // even if draining, unless we are draining for destroy.
        if (drain_type_ == DRAIN_FOR_DESTROY)
          return true;  // ignore

        if (media_codec_->GetOutputSize(&size_) != MEDIA_CODEC_OK) {
          NOTIFY_ERROR(PLATFORM_FAILURE, "GetOutputSize failed.");
          return false;
        }

        DVLOG(3) << __func__
                 << " OUTPUT_FORMAT_CHANGED, new size: " << size_.ToString();

        // Don't request picture buffers if we already have some. This avoids
        // having to dismiss the existing buffers which may actively reference
        // decoded images. Breaking their connection to the decoded image will
        // cause rendering of black frames. Instead, we let the existing
        // PictureBuffers live on and we simply update their size the next time
        // they're attached to an image of the new resolution. See the
        // size update in |SendDecodedFrameToClient| and https://crbug/587994.
        if (output_picture_buffers_.empty() && !picturebuffers_requested_) {
          picturebuffers_requested_ = true;
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE,
              base::Bind(&AndroidVideoDecodeAccelerator::RequestPictureBuffers,
                         weak_this_factory_.GetWeakPtr()));
          return false;
        }

        return true;
      }

      case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
        break;

      case MEDIA_CODEC_OK:
        DCHECK_GE(buf_index, 0);
        DVLOG(3) << __func__ << ": pts:" << presentation_timestamp
                 << " buf_index:" << buf_index << " offset:" << offset
                 << " size:" << size << " eos:" << eos;
        break;

      default:
        NOTREACHED();
        break;
    }
  } while (buf_index < 0);

  if (eos) {
    OnDrainCompleted();
    return false;
  }

  if (IsDrainingForResetOrDestroy()) {
    media_codec_->ReleaseOutputBuffer(buf_index, false);
    return true;
  }

  if (!picturebuffers_requested_) {
    // In 0.01% of playbacks MediaCodec returns a frame before FORMAT_CHANGED.
    // Occurs on JB and M. (See the Media.AVDA.MissingFormatChanged histogram.)
    media_codec_->ReleaseOutputBuffer(buf_index, false);
    NOTIFY_ERROR(PLATFORM_FAILURE, "Dequeued buffers before FORMAT_CHANGED.");
    return false;
  }

  // Get the bitstream buffer id from the timestamp.
  auto it = bitstream_buffers_in_decoder_.find(presentation_timestamp);

  if (it != bitstream_buffers_in_decoder_.end()) {
    const int32_t bitstream_buffer_id = it->second;
    bitstream_buffers_in_decoder_.erase(bitstream_buffers_in_decoder_.begin(),
                                        ++it);
    SendDecodedFrameToClient(buf_index, bitstream_buffer_id);

    // Removes ids former or equal than the id from decoder. Note that
    // |bitstreams_notified_in_advance_| does not mean bitstream ids in decoder
    // because of frame reordering issue. We just maintain this roughly and use
    // it for throttling.
    for (auto bitstream_it = bitstreams_notified_in_advance_.begin();
         bitstream_it != bitstreams_notified_in_advance_.end();
         ++bitstream_it) {
      if (*bitstream_it == bitstream_buffer_id) {
        bitstreams_notified_in_advance_.erase(
            bitstreams_notified_in_advance_.begin(), ++bitstream_it);
        break;
      }
    }
  } else {
    // Normally we assume that the decoder makes at most one output frame for
    // each distinct input timestamp. However MediaCodecBridge uses timestamp
    // correction and provides a non-decreasing timestamp sequence, which might
    // result in timestamp duplicates. Discard the frame if we cannot get the
    // corresponding buffer id.
    DVLOG(3) << __func__ << ": Releasing buffer with unexpected PTS: "
             << presentation_timestamp;
    media_codec_->ReleaseOutputBuffer(buf_index, false);
  }

  // We got a decoded frame, so try for another.
  return true;
}

void AndroidVideoDecodeAccelerator::SendDecodedFrameToClient(
    int32_t codec_buffer_index,
    int32_t bitstream_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(bitstream_id, -1);
  DCHECK(!free_picture_ids_.empty());
  TRACE_EVENT0("media", "AVDA::SendDecodedFrameToClient");

  if (!make_context_current_cb_.Run()) {
    NOTIFY_ERROR(PLATFORM_FAILURE, "Failed to make the GL context current.");
    return;
  }

  int32_t picture_buffer_id = free_picture_ids_.front();
  free_picture_ids_.pop();
  TRACE_COUNTER1("media", "AVDA::FreePictureIds", free_picture_ids_.size());

  const auto it = output_picture_buffers_.find(picture_buffer_id);
  if (it == output_picture_buffers_.end()) {
    NOTIFY_ERROR(PLATFORM_FAILURE,
                 "Can't find PictureBuffer id: " << picture_buffer_id);
    return;
  }

  PictureBuffer& picture_buffer = it->second;
  const bool size_changed = picture_buffer.size() != size_;
  if (size_changed)
    picture_buffer.set_size(size_);

  // Only ask for promotion hints if we can actually switch surfaces.
  const bool want_promotion_hint = device_info_->IsSetOutputSurfaceSupported();
  const bool allow_overlay = picture_buffer_manager_.ArePicturesOverlayable();

  // TODO(liberato): remove in M63, if FrameInformation is clearly working.
  UMA_HISTOGRAM_BOOLEAN("Media.AVDA.FrameSentAsOverlay", allow_overlay);

  // Record the frame type that we're sending and some information about why.
  UMA_HISTOGRAM_ENUMERATION(
      "Media.AVDA.FrameInformation", cached_frame_information_,
      static_cast<int>(
          SurfaceChooserHelper::FrameInformation::FRAME_INFORMATION_MAX) +
          1);  // PRESUBMIT_IGNORE_UMA_MAX

  // We unconditionally mark the picture as overlayable, even if
  // |!allow_overlay|, if we want to get hints.  It's required, else we won't
  // get hints.
  // TODO(hubbe): Insert the correct color space. http://crbug.com/647725
  Picture picture(picture_buffer_id, bitstream_id, gfx::Rect(size_),
                  gfx::ColorSpace(),
                  want_promotion_hint ? true : allow_overlay);
  picture.set_size_changed(size_changed);
  if (want_promotion_hint) {
    picture.set_wants_promotion_hint(true);
    // This will prevent it from actually being promoted if it shouldn't be.
    picture.set_texture_owner(!allow_overlay);
  }

  // Notify picture ready before calling UseCodecBufferForPictureBuffer() since
  // that process may be slow and shouldn't delay delivery of the frame to the
  // renderer. The picture is only used on the same thread as this method is
  // called, so it is safe to do this.
  NotifyPictureReady(picture);

  // Connect the PictureBuffer to the decoded frame.
  picture_buffer_manager_.UseCodecBufferForPictureBuffer(codec_buffer_index,
                                                         picture_buffer);
}

void AndroidVideoDecodeAccelerator::Decode(
    const BitstreamBuffer& bitstream_buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If we deferred getting a surface, then start getting one now.
  if (defer_surface_creation_) {
    // We should still be in BEFORE_OVERLAY_INIT, since we've deferred doing it
    // until now.
    DCHECK_EQ(state_, BEFORE_OVERLAY_INIT);
    defer_surface_creation_ = false;
    StartSurfaceChooser();
    if (state_ == ERROR) {
      DLOG(ERROR) << "Failed deferred surface and MediaCodec initialization.";
      return;
    }
  }

  // If we previously deferred a codec restart, take care of it now. This can
  // happen on older devices where configuration changes require a codec reset.
  if (codec_needs_reset_) {
    DCHECK(!drain_type_);
    ResetCodecState();
  }

  if (bitstream_buffer.id() >= 0 && bitstream_buffer.size() > 0) {
    DecodeBuffer(bitstream_buffer);
    return;
  }

  if (base::SharedMemory::IsHandleValid(bitstream_buffer.handle()))
    base::SharedMemory::CloseHandle(bitstream_buffer.handle());

  if (bitstream_buffer.id() < 0) {
    NOTIFY_ERROR(INVALID_ARGUMENT,
                 "Invalid bistream_buffer, id: " << bitstream_buffer.id());
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer,
                   weak_this_factory_.GetWeakPtr(), bitstream_buffer.id()));
  }
}

void AndroidVideoDecodeAccelerator::DecodeBuffer(
    const BitstreamBuffer& bitstream_buffer) {
  pending_bitstream_records_.push(BitstreamRecord(bitstream_buffer));
  TRACE_COUNTER1("media", "AVDA::PendingBitstreamBufferCount",
                 pending_bitstream_records_.size());

  DoIOTask(true);
}

void AndroidVideoDecodeAccelerator::RequestPictureBuffers() {
  if (client_) {
    // Allocate a picture buffer that is the actual frame size.  Note that it
    // will be an external texture anyway, so it doesn't allocate an image of
    // that size.  It's important to get the coded size right, so that
    // VideoLayerImpl doesn't try to scale the texture when building the quad
    // for it.
    client_->ProvidePictureBuffers(kNumPictureBuffers, PIXEL_FORMAT_UNKNOWN, 1,
                                   size_,
                                   AVDAPictureBufferManager::kTextureTarget);
  }
}

void AndroidVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<PictureBuffer>& buffers) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(output_picture_buffers_.empty());
  DCHECK(free_picture_ids_.empty());

  if (buffers.size() < kNumPictureBuffers) {
    NOTIFY_ERROR(INVALID_ARGUMENT, "Not enough picture buffers assigned.");
    return;
  }

  const bool have_context = make_context_current_cb_.Run();
  LOG_IF(WARNING, !have_context)
      << "Failed to make GL context current for Assign, continuing.";

  for (size_t i = 0; i < buffers.size(); ++i) {
    DCHECK(buffers[i].size() == size_);
    int32_t id = buffers[i].id();
    output_picture_buffers_.insert(std::make_pair(id, buffers[i]));
    free_picture_ids_.push(id);

    picture_buffer_manager_.AssignOnePictureBuffer(buffers[i], have_context);
  }
  TRACE_COUNTER1("media", "AVDA::FreePictureIds", free_picture_ids_.size());
  DoIOTask(true);
}

void AndroidVideoDecodeAccelerator::ReusePictureBuffer(
    int32_t picture_buffer_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  free_picture_ids_.push(picture_buffer_id);
  TRACE_COUNTER1("media", "AVDA::FreePictureIds", free_picture_ids_.size());

  auto it = output_picture_buffers_.find(picture_buffer_id);
  if (it == output_picture_buffers_.end()) {
    NOTIFY_ERROR(PLATFORM_FAILURE,
                 "Can't find PictureBuffer id " << picture_buffer_id);
    return;
  }

  picture_buffer_manager_.ReuseOnePictureBuffer(it->second);
  DoIOTask(true);
}

void AndroidVideoDecodeAccelerator::Flush() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  StartCodecDrain(DRAIN_FOR_FLUSH);
}

void AndroidVideoDecodeAccelerator::ConfigureMediaCodecAsynchronously() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!media_codec_);
  DCHECK_NE(state_, WAITING_FOR_CODEC);
  state_ = WAITING_FOR_CODEC;

  codec_allocator_->CreateMediaCodecAsync(weak_this_factory_.GetWeakPtr(),
                                          codec_config_);
}

void AndroidVideoDecodeAccelerator::ConfigureMediaCodecSynchronously() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!media_codec_);
  DCHECK_NE(state_, WAITING_FOR_CODEC);
  state_ = WAITING_FOR_CODEC;

  std::unique_ptr<MediaCodecBridge> media_codec =
      codec_allocator_->CreateMediaCodecSync(codec_config_);
  OnCodecConfigured(std::move(media_codec), codec_config_->surface_bundle);
}

void AndroidVideoDecodeAccelerator::OnCodecConfigured(
    std::unique_ptr<MediaCodecBridge> media_codec,
    scoped_refptr<AVDASurfaceBundle> surface_bundle) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(state_ == WAITING_FOR_CODEC || state_ == SURFACE_DESTROYED);
  // If we are supposed to notify that initialization is complete, then do so
  // before returning.  Otherwise, this is a reconfiguration.

  DCHECK(!media_codec_);
  media_codec_ = std::move(media_codec);

  // If |state_| changed to SURFACE_DESTROYED while we were configuring a codec,
  // then the codec is already invalid so we return early and drop it.
  if (state_ == SURFACE_DESTROYED) {
    if (deferred_initialization_pending_) {
      // Losing the output surface is not considered an error state, so notify
      // success. The client will destroy |this| soon.
      NotifyInitializationSucceeded();
    }

    // Post it to the right thread.
    ReleaseCodecAndBundle();
    return;
  }

  picture_buffer_manager_.CodecChanged(media_codec_.get());
  if (!media_codec_) {
    NOTIFY_ERROR(PLATFORM_FAILURE, "Failed to create MediaCodec");
    return;
  }

  if (deferred_initialization_pending_)
    NotifyInitializationSucceeded();

  state_ = NO_ERROR;

  ManageTimer(true);
}

void AndroidVideoDecodeAccelerator::StartCodecDrain(DrainType drain_type) {
  DVLOG(2) << __func__ << " drain_type:" << drain_type;
  DCHECK(thread_checker_.CalledOnValidThread());

  auto previous_drain_type = drain_type_;
  drain_type_ = drain_type;

  // Only DRAIN_FOR_DESTROY is allowed while a drain is already in progress.
  DCHECK(!previous_drain_type || drain_type == DRAIN_FOR_DESTROY)
      << "StartCodecDrain(" << drain_type
      << ") while already draining with type " << previous_drain_type.value();

  // Skip the drain if:
  // * There's no codec.
  // * The codec is not currently decoding and we have no more inputs to submit.
  //   (Reset() and Destroy() should clear pending inputs before calling this).
  // * The drain is for reset or destroy (where we can drop pending decodes) and
  //   the codec is not VP8. We still have to drain VP8 in this case because
  //   MediaCodec can hang in release() or flush() if we don't drain it.
  //   http://crbug.com/598963
  if (!media_codec_ ||
      (pending_bitstream_records_.empty() &&
       bitstream_buffers_in_decoder_.empty()) ||
      (drain_type != DRAIN_FOR_FLUSH && codec_config_->codec != kCodecVP8)) {
    OnDrainCompleted();
    return;
  }

  // Queue EOS if one is not already queued.
  if (!previous_drain_type)
    DecodeBuffer(BitstreamBuffer(-1, base::SharedMemoryHandle(), 0));
}

bool AndroidVideoDecodeAccelerator::IsDrainingForResetOrDestroy() const {
  return drain_type_ == DRAIN_FOR_RESET || drain_type_ == DRAIN_FOR_DESTROY;
}

void AndroidVideoDecodeAccelerator::OnDrainCompleted() {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // Sometimes MediaCodec returns an EOS buffer even if we didn't queue one.
  // Consider it an error. http://crbug.com/585959.
  if (!drain_type_) {
    NOTIFY_ERROR(PLATFORM_FAILURE, "Unexpected EOS");
    return;
  }

  switch (*drain_type_) {
    case DRAIN_FOR_FLUSH:
      ResetCodecState();
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&AndroidVideoDecodeAccelerator::NotifyFlushDone,
                         weak_this_factory_.GetWeakPtr()));
      break;
    case DRAIN_FOR_RESET:
      ResetCodecState();
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&AndroidVideoDecodeAccelerator::NotifyResetDone,
                         weak_this_factory_.GetWeakPtr()));
      break;
    case DRAIN_FOR_DESTROY:
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&AndroidVideoDecodeAccelerator::ActualDestroy,
                         weak_this_factory_.GetWeakPtr()));
      break;
  }
  drain_type_.reset();
}

void AndroidVideoDecodeAccelerator::ResetCodecState() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If there is already a reset in flight, then that counts.  This can really
  // only happen if somebody calls Reset.
  // If the surface is destroyed or we're in an error state there's nothing to
  // do. Note that BEFORE_OVERLAY_INIT implies that we have no codec, but it's
  // included for completeness.
  if (state_ == WAITING_FOR_CODEC || state_ == SURFACE_DESTROYED ||
      state_ == BEFORE_OVERLAY_INIT || state_ == ERROR || !media_codec_) {
    return;
  }

  bitstream_buffers_in_decoder_.clear();

  if (pending_input_buf_index_ != -1) {
    // The data for that index exists in the input buffer, but corresponding
    // shm block been deleted. Check that it is safe to flush the codec, i.e.
    // |pending_bitstream_records_| is empty.
    // TODO(timav): keep shm block for that buffer and remove this restriction.
    DCHECK(pending_bitstream_records_.empty());
    pending_input_buf_index_ = -1;
  }

  // If we've just completed a flush don't reset the codec yet. Instead defer
  // until the next decode call. This prevents us from unbacking frames that
  // might be out for display at end of stream.
  codec_needs_reset_ =
      (drain_type_ == DRAIN_FOR_FLUSH) || (drain_type_ == DRAIN_FOR_RESET);
  if (codec_needs_reset_)
    return;

  // Flush the codec if possible, or create a new one if not.
  if (!MediaCodecUtil::CodecNeedsFlushWorkaround(media_codec_.get())) {
    DVLOG(3) << __func__ << " Flushing MediaCodec.";
    media_codec_->Flush();
    // Since we just flushed all the output buffers, make sure that nothing is
    // using them.
    picture_buffer_manager_.CodecChanged(media_codec_.get());
  } else {
    DVLOG(3) << __func__ << " Deleting the MediaCodec and creating a new one.";
    GetManager()->StopTimer(this);
    // Release the codec, retain the bundle, and allocate a new codec.  It will
    // not wait for the old one to finish up with the bundle, which is bad.  It
    // works (usually) because it ends up allocating the codec on the same
    // thread as is used to release the old one, so it's serialized anyway.
    ReleaseCodec();
    ConfigureMediaCodecAsynchronously();
  }
}

void AndroidVideoDecodeAccelerator::Reset() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("media", "AVDA::Reset");

  if (defer_surface_creation_) {
    DCHECK(!media_codec_);
    DCHECK(pending_bitstream_records_.empty());
    DCHECK_EQ(state_, BEFORE_OVERLAY_INIT);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AndroidVideoDecodeAccelerator::NotifyResetDone,
                       weak_this_factory_.GetWeakPtr()));
    return;
  }

  while (!pending_bitstream_records_.empty()) {
    int32_t bitstream_buffer_id =
        pending_bitstream_records_.front().buffer.id();
    pending_bitstream_records_.pop();

    if (bitstream_buffer_id != -1) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer,
                     weak_this_factory_.GetWeakPtr(), bitstream_buffer_id));
    }
  }
  TRACE_COUNTER1("media", "AVDA::PendingBitstreamBufferCount", 0);
  bitstreams_notified_in_advance_.clear();

  picture_buffer_manager_.ReleaseCodecBuffers(output_picture_buffers_);
  StartCodecDrain(DRAIN_FOR_RESET);
}

void AndroidVideoDecodeAccelerator::SetOverlayInfo(
    const OverlayInfo& overlay_info) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == ERROR)
    return;

  // Update |config_| to contain the most recent info.  Also save a copy, so
  // that we can check for duplicate info later.
  OverlayInfo previous_info = config_.overlay_info;
  config_.overlay_info = overlay_info;

  // It's possible that we'll receive SetSurface before initializing the surface
  // chooser.  For example, if we defer surface creation, then we'll signal
  // success to WMPI before initializing it.  WMPI is then free to change
  // |surface_id|.  In this case, take no additional action, since |config_| is
  // up to date.  We'll use it later.
  if (state_ == BEFORE_OVERLAY_INIT)
    return;

  // Notify the chooser about the fullscreen state.
  surface_chooser_helper_.SetIsFullscreen(overlay_info.is_fullscreen);

  // Note that these might be kNoSurfaceID / empty.  In that case, we will
  // revoke the factory.
  OverlayInfo::RoutingToken routing_token = overlay_info.routing_token;

  // We don't want to change the factory unless this info has actually changed.
  // We'll get the same info many times if some other part of the config is now
  // different, such as fullscreen state.
  base::Optional<AndroidOverlayFactoryCB> new_factory;
  if (routing_token != previous_info.routing_token) {
    if (routing_token && overlay_factory_cb_)
      new_factory = base::BindRepeating(overlay_factory_cb_, *routing_token);
  }

  surface_chooser_helper_.UpdateChooserState(new_factory);
}

void AndroidVideoDecodeAccelerator::Destroy() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  picture_buffer_manager_.Destroy(output_picture_buffers_);
  client_ = nullptr;

  // We don't want to queue more inputs while draining.
  base::queue<BitstreamRecord>().swap(pending_bitstream_records_);
  StartCodecDrain(DRAIN_FOR_DESTROY);
}

void AndroidVideoDecodeAccelerator::ActualDestroy() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // Note that async codec construction might still be in progress.  In that
  // case, the codec will be deleted when it completes once we invalidate all
  // our weak refs.
  weak_this_factory_.InvalidateWeakPtrs();
  GetManager()->StopTimer(this);
  // We only release the codec here, in case codec allocation is in progress.
  // We don't want to modify |codec_config_|.  Note that the ref will sill be
  // dropped when it completes, or when we delete |this|.
  ReleaseCodec();

  delete this;
}

bool AndroidVideoDecodeAccelerator::TryToSetupDecodeOnSeparateThread(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner) {
  return false;
}

const gfx::Size& AndroidVideoDecodeAccelerator::GetSize() const {
  return size_;
}

gpu::gles2::ContextGroup* AndroidVideoDecodeAccelerator::GetContextGroup()
    const {
  return get_context_group_cb_.Run();
}

void AndroidVideoDecodeAccelerator::OnStopUsingOverlayImmediately(
    AndroidOverlay* overlay) {
  DVLOG(1) << __func__;
  TRACE_EVENT0("media", "AVDA::OnStopUsingOverlayImmediately");
  DCHECK(thread_checker_.CalledOnValidThread());

  // We cannot get here if we're before surface allocation, since we transition
  // to WAITING_FOR_CODEC (or NO_ERROR, if sync) when we get the surface without
  // posting.  If we do ever lose the surface before starting codec allocation,
  // then we could just update the config to use a TextureOwner and return
  // without changing state.
  DCHECK_NE(state_, BEFORE_OVERLAY_INIT);

  // If we're transitioning to |overlay|, then just stop here.  We're not also
  // using the overlay if we're transitioning to it.
  if (!!incoming_overlay_ && incoming_overlay_->get() == overlay) {
    incoming_overlay_.reset();
    return;
  }

  // If we have no codec, or if our current config doesn't refer to |overlay|,
  // then do nothing.  |overlay| might be for some overlay that's waiting for
  // codec destruction on some other thread.
  if (!codec_config_->surface_bundle ||
      codec_config_->surface_bundle->overlay.get() != overlay) {
    return;
  }

  // If we have a codec, or if codec allocation is in flight, then it's using an
  // overlay that was destroyed.
  if (state_ == WAITING_FOR_CODEC) {
    // What we should do here is to set |incoming_overlay_| to nullptr, to start
    // a transistion to TextureOwner.  OnCodecConfigured could notice that
    // there's an incoming overlay, and then immediately transition the codec /
    // drop and re-allocate the codec using it.  However, for CVV, that won't
    // work, since CVV-based overlays block the main thread waiting for the
    // overlay to be dropped, so OnCodecConfigured won't run.  For DS, it's the
    // right thing.
    // So, for now, we just fail, and let OnCodecConfigured drop the codec.
    // Note that this case really can only happen on pre-M anyway, unless it's
    // during initial construction.  This will result in the overlay being
    // destroyed after timeout, since OnCodecConfigured can't run until the
    // synchronous CVV destruction quits.
    state_ = SURFACE_DESTROYED;
    return;
  }

  // If the API is available avoid having to restart the decoder in order to
  // leave fullscreen. If we don't clear the surface immediately during this
  // callback, the MediaCodec will throw an error as the surface is destroyed.
  if (device_info_->IsSetOutputSurfaceSupported()) {
    // Since we can't wait for a transition, we must invalidate all outstanding
    // picture buffers to avoid putting the GL system in a broken state.
    picture_buffer_manager_.ReleaseCodecBuffers(output_picture_buffers_);

    // If we aren't transitioning to some other surface, then transition to a
    // TextureOwner.  Remember that, if |incoming_overlay_| is an overlay,
    // then it's already ready and can be transitioned to immediately.  We were
    // just waiting for codec buffers to come back, but we just dropped them.
    // Note that we want |incoming_overlay_| to has_value(), but that value
    // should be a nullptr to indicate that we should switch to TextureOwner.
    if (!incoming_overlay_)
      incoming_overlay_ = std::unique_ptr<AndroidOverlay>();

    UpdateSurface();
    // Switching to a TextureOwner should never need to wait.  If it does,
    // then the codec might still be using the destroyed surface, which is bad.
    return;
  }

  // If we're currently asynchronously configuring a codec, it will be destroyed
  // when configuration completes and it notices that |state_| has changed to
  // SURFACE_DESTROYED.  It's safe to modify |codec_config_| here, since we
  // checked above for WAITING_FOR_CODEC.
  state_ = SURFACE_DESTROYED;
  ReleaseCodecAndBundle();

  // If we're draining, signal completion now because the drain can no longer
  // proceed.
  if (drain_type_)
    OnDrainCompleted();
}

void AndroidVideoDecodeAccelerator::InitializeCdm() {
  DVLOG(2) << __func__ << ": " << config_.cdm_id;

#if !BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  NOTREACHED();
#else
  // Store the CDM to hold a reference to it.
  cdm_for_reference_holding_only_ =
      CdmManager::GetInstance()->GetCdm(config_.cdm_id);

  // We can DCHECK here and below because we checked HasValidCdm() before
  // calling InitializeCdm(), and the status shouldn't have changed since then.
  DCHECK(cdm_for_reference_holding_only_) << "CDM not available";

  media_crypto_context_ =
      cdm_for_reference_holding_only_->GetCdmContext()->GetMediaCryptoContext();
  DCHECK(media_crypto_context_) << "MediaCryptoContext not available.";

  // Deferred initialization will continue in OnMediaCryptoReady(). The callback
  // registered will be posted back to this thread via BindToCurrentLoop.
  media_crypto_context_->SetMediaCryptoReadyCB(BindToCurrentLoop(
      base::Bind(&AndroidVideoDecodeAccelerator::OnMediaCryptoReady,
                 weak_this_factory_.GetWeakPtr())));
#endif  // !BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
}

void AndroidVideoDecodeAccelerator::OnMediaCryptoReady(
    JavaObjectPtr media_crypto,
    bool requires_secure_video_codec) {
  DVLOG(1) << __func__;
  DCHECK(media_crypto);

  if (media_crypto->is_null()) {
    media_crypto_context_->SetMediaCryptoReadyCB(
        MediaCryptoContext::MediaCryptoReadyCB());
    media_crypto_context_ = nullptr;
    cdm_for_reference_holding_only_ = nullptr;

    if (config_.is_encrypted()) {
      LOG(ERROR)
          << "MediaCrypto is not available, can't play encrypted stream.";
      NOTIFY_ERROR(PLATFORM_FAILURE, "MediaCrypto is not available");
      return;
    }

    // MediaCrypto is not available, but the stream is clear. So we can still
    // play the current stream. But if we switch to an encrypted stream playback
    // will fail.
    StartSurfaceChooser();
    return;
  }

  // We assume this is a part of the initialization process, thus MediaCodec
  // is not created yet.
  DCHECK(!media_codec_);
  DCHECK(deferred_initialization_pending_);

  // Since |this| holds a reference to the |cdm_|, by the time the CDM is
  // destructed, UnregisterPlayer() must have been called and |this| has been
  // destructed as well. So the |cdm_unset_cb| will never have a chance to be
  // called.
  // TODO(xhwang): Remove |cdm_unset_cb| after it's not used on all platforms.
  cdm_registration_id_ = media_crypto_context_->RegisterPlayer(
      BindToCurrentLoop(base::Bind(&AndroidVideoDecodeAccelerator::OnKeyAdded,
                                   weak_this_factory_.GetWeakPtr())),
      base::DoNothing());

  codec_config_->media_crypto = std::move(media_crypto);
  codec_config_->requires_secure_codec = requires_secure_video_codec;

  // Request a secure surface in all cases.  For L3, it's okay if we fall back
  // to TextureOwner rather than fail composition.  For L1, it's required.
  // It's also required if the command line says so.
  surface_chooser_helper_.SetSecureSurfaceMode(
      requires_secure_video_codec
          ? SurfaceChooserHelper::SecureSurfaceMode::kRequired
          : SurfaceChooserHelper::SecureSurfaceMode::kRequested);

  // After receiving |media_crypto_| we can start with surface creation.
  StartSurfaceChooser();
}

void AndroidVideoDecodeAccelerator::OnKeyAdded() {
  DVLOG(1) << __func__;

  // This can also be called before initial surface allocation has completed,
  // so we might not have a surface / codec yet.  In that case, we'll never
  // transition to WAITING_FOR_KEY, which is fine.
  if (state_ == WAITING_FOR_KEY)
    state_ = NO_ERROR;

  DoIOTask(true);
}

void AndroidVideoDecodeAccelerator::NotifyInitializationSucceeded() {
  DCHECK(deferred_initialization_pending_);

  if (client_)
    client_->NotifyInitializationComplete(true);
  deferred_initialization_pending_ = false;
}

void AndroidVideoDecodeAccelerator::NotifyPictureReady(const Picture& picture) {
  if (client_)
    client_->PictureReady(picture);
}

void AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer(
    int input_buffer_id) {
  if (client_)
    client_->NotifyEndOfBitstreamBuffer(input_buffer_id);
}

void AndroidVideoDecodeAccelerator::NotifyFlushDone() {
  if (client_)
    client_->NotifyFlushDone();
}

void AndroidVideoDecodeAccelerator::NotifyResetDone() {
  if (client_)
    client_->NotifyResetDone();
}

void AndroidVideoDecodeAccelerator::NotifyError(Error error) {
  state_ = ERROR;

  // If we're in the middle of Initialize, then stop.  It will notice |state_|.
  if (during_initialize_)
    return;

  // If deferred init is pending, then notify the client that it failed.
  if (deferred_initialization_pending_) {
    if (client_)
      client_->NotifyInitializationComplete(false);
    deferred_initialization_pending_ = false;
    return;
  }

  // We're after all init.  Just signal an error.
  if (client_)
    client_->NotifyError(error);
}

PromotionHintAggregator::NotifyPromotionHintCB
AndroidVideoDecodeAccelerator::GetPromotionHintCB() {
  return base::Bind(&AndroidVideoDecodeAccelerator::NotifyPromotionHint,
                    weak_this_factory_.GetWeakPtr());
}

void AndroidVideoDecodeAccelerator::NotifyPromotionHint(
    PromotionHintAggregator::Hint hint) {
  bool is_using_overlay =
      codec_config_->surface_bundle && codec_config_->surface_bundle->overlay;
  surface_chooser_helper_.NotifyPromotionHintAndUpdateChooser(hint,
                                                              is_using_overlay);
}

void AndroidVideoDecodeAccelerator::ManageTimer(bool did_work) {
  bool should_be_running = true;

  base::TimeTicks now = base::TimeTicks::Now();
  if (!did_work && !most_recent_work_.is_null()) {
    // Make sure that we have done work recently enough, else stop the timer.
    if (now - most_recent_work_ > IdleTimerTimeOut) {
      most_recent_work_ = base::TimeTicks();
      should_be_running = false;
    }
  } else {
    most_recent_work_ = now;
  }

  if (should_be_running)
    GetManager()->StartTimer(this);
  else
    GetManager()->StopTimer(this);
}

// static
VideoDecodeAccelerator::Capabilities
AndroidVideoDecodeAccelerator::GetCapabilities(
    const gpu::GpuPreferences& gpu_preferences) {
  Capabilities capabilities;
  SupportedProfiles& profiles = capabilities.supported_profiles;

  if (MediaCodecUtil::IsVp8DecoderAvailable()) {
    SupportedProfile profile;
    profile.profile = VP8PROFILE_ANY;
    // Since there is little to no power benefit below 360p, don't advertise
    // support for it.  Let libvpx decode it, and save a MediaCodec instance.
    // Note that we allow it anyway for encrypted content, since we push a
    // separate profile for that.
    profile.min_resolution.SetSize(480, 360);
    profile.max_resolution.SetSize(3840, 2160);
    // If we know MediaCodec will just create a software codec, prefer our
    // internal software decoder instead. It's more up to date and secured
    // within the renderer sandbox. However if the content is encrypted, we
    // must use MediaCodec anyways since MediaDrm offers no way to decrypt
    // the buffers and let us use our internal software decoders.
    profile.encrypted_only = MediaCodecUtil::IsKnownUnaccelerated(
        kCodecVP8, MediaCodecDirection::DECODER);
    profiles.push_back(profile);

    // Always allow encrypted content, even at low resolutions.
    profile.min_resolution.SetSize(0, 0);
    profile.encrypted_only = true;
    profiles.push_back(profile);
  }

  if (MediaCodecUtil::IsVp9DecoderAvailable()) {
    const VideoCodecProfile profile_types[] = {
        VP9PROFILE_PROFILE0, VP9PROFILE_PROFILE1, VP9PROFILE_PROFILE2,
        VP9PROFILE_PROFILE3, VIDEO_CODEC_PROFILE_UNKNOWN};
    const bool is_known_unaccelerated = MediaCodecUtil::IsKnownUnaccelerated(
        kCodecVP9, MediaCodecDirection::DECODER);
    for (int i = 0; profile_types[i] != VIDEO_CODEC_PROFILE_UNKNOWN; i++) {
      SupportedProfile profile;
      // Limit to 360p, like we do for vp8.  See above.
      profile.min_resolution.SetSize(480, 360);
      profile.max_resolution.SetSize(3840, 2160);
      // If we know MediaCodec will just create a software codec, prefer our
      // internal software decoder instead. It's more up to date and secured
      // within the renderer sandbox. However if the content is encrypted, we
      // must use MediaCodec anyways since MediaDrm offers no way to decrypt
      // the buffers and let us use our internal software decoders.
      profile.encrypted_only = is_known_unaccelerated;
      profile.profile = profile_types[i];
      profiles.push_back(profile);

      // Always allow encrypted content.
      profile.min_resolution.SetSize(0, 0);
      profile.encrypted_only = true;
      profiles.push_back(profile);
    }
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  for (const auto& supported_profile : kSupportedH264Profiles) {
    SupportedProfile profile;
    profile.profile = supported_profile;
    profile.min_resolution.SetSize(0, 0);
    // Advertise support for 4k and let the MediaCodec fail when decoding if it
    // doesn't support the resolution. It's assumed that consumers won't have
    // software fallback for H264 on Android anyway.
    profile.max_resolution.SetSize(3840, 2160);
    profiles.push_back(profile);
  }

#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
  for (const auto& supported_profile : kSupportedHevcProfiles) {
    SupportedProfile profile;
    profile.profile = supported_profile;
    profile.min_resolution.SetSize(0, 0);
    profile.max_resolution.SetSize(3840, 2160);
    profiles.push_back(profile);
  }
#endif
#endif

  capabilities.flags = Capabilities::SUPPORTS_DEFERRED_INITIALIZATION |
                       Capabilities::NEEDS_ALL_PICTURE_BUFFERS_TO_DECODE |
                       Capabilities::SUPPORTS_ENCRYPTED_STREAMS;

  // If we're using threaded texture mailboxes the COPY_REQUIRED flag must be
  // set on the video frames (http://crbug.com/582170), and SurfaceView output
  // is disabled (http://crbug.com/582170).
  if (gpu_preferences.enable_threaded_texture_mailboxes) {
    capabilities.flags |= Capabilities::REQUIRES_TEXTURE_COPY;
  } else if (MediaCodecUtil::IsSurfaceViewOutputSupported()) {
    capabilities.flags |= Capabilities::SUPPORTS_EXTERNAL_OUTPUT_SURFACE;
    if (MediaCodecUtil::IsSetOutputSurfaceSupported())
      capabilities.flags |= Capabilities::SUPPORTS_SET_EXTERNAL_OUTPUT_SURFACE;
  }

  return capabilities;
}

bool AndroidVideoDecodeAccelerator::IsMediaCodecSoftwareDecodingForbidden()
    const {
  // Prevent MediaCodec from using its internal software decoders when we have
  // more secure and up to date versions in the renderer process.
  return !config_.is_encrypted() &&
         (codec_config_->codec == kCodecVP8 ||
          codec_config_->codec == kCodecVP9) &&
         !force_allow_software_decoding_for_testing_;
}

bool AndroidVideoDecodeAccelerator::UpdateSurface() {
  DCHECK(incoming_overlay_);
  DCHECK_NE(state_, WAITING_FOR_CODEC);

  // Start surface creation.  Note that if we're called via surfaceDestroyed,
  // then this must complete synchronously or it will DCHECK.  Otherwise, we
  // might still be using the destroyed surface.  We don't enforce this, but
  // it's worth remembering that there are cases where it's required.
  // Note that we don't re-use |surface_bundle|, since the codec is using it!
  incoming_bundle_ =
      new AVDASurfaceBundle(std::move(incoming_overlay_.value()));
  incoming_overlay_.reset();
  InitializePictureBufferManager();
  if (state_ == ERROR) {
    // This might be called from OnSurfaceDestroyed(), so we have to release the
    // MediaCodec if we failed to switch the surface.  We reset the surface ID
    // to the previous one, since failures never result in the codec using the
    // new surface.  This is only guaranteed because of how OnCodecConfigured
    // works.  If it could fail after getting a codec, then this assumption
    // wouldn't be necessarily true anymore.
    // Also note that we might not have switched surfaces yet, which is also bad
    // for OnSurfaceDestroyed, because of BEFORE_OVERLAY_INIT.  Shouldn't
    // happen with TextureOwner, and OnSurfaceDestroyed checks for it.  In
    // either case, we definitely should not still have an incoming bundle; it
    // should have been dropped.
    DCHECK(!incoming_bundle_);
    ReleaseCodecAndBundle();
  }

  return state_ != ERROR;
}

void AndroidVideoDecodeAccelerator::ReleaseCodec() {
  if (!media_codec_)
    return;

  picture_buffer_manager_.CodecChanged(nullptr);
  codec_allocator_->ReleaseMediaCodec(std::move(media_codec_),
                                      codec_config_->surface_bundle);
}

void AndroidVideoDecodeAccelerator::ReleaseCodecAndBundle() {
  ReleaseCodec();
  codec_config_->surface_bundle = nullptr;
}

void AndroidVideoDecodeAccelerator::CacheFrameInformation() {
  bool is_using_overlay =
      codec_config_->surface_bundle && codec_config_->surface_bundle->overlay;

  cached_frame_information_ =
      surface_chooser_helper_.ComputeFrameInformation(is_using_overlay);
}

}  // namespace media
