// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_ANDROID_VIDEO_DECODE_ACCELERATOR_H_
#define MEDIA_GPU_ANDROID_ANDROID_VIDEO_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <list>
#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/android/media_codec_bridge_impl.h"
#include "media/base/android/media_crypto_context.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "media/base/content_decryption_module.h"
#include "media/gpu/android/avda_picture_buffer_manager.h"
#include "media/gpu/android/avda_state_provider.h"
#include "media/gpu/android/codec_allocator.h"
#include "media/gpu/android/device_info.h"
#include "media/gpu/android/surface_chooser_helper.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"

namespace media {
class AndroidVideoSurfaceChooser;
class PromotionHintAggregator;

// A VideoDecodeAccelerator implementation for Android. This class decodes the
// encoded input stream using Android's MediaCodec. It handles the work of
// transferring data to and from MediaCodec, and delegates attaching MediaCodec
// output buffers to PictureBuffers to AVDAPictureBufferManager.
class MEDIA_GPU_EXPORT AndroidVideoDecodeAccelerator
    : public VideoDecodeAccelerator,
      public AVDAStateProvider,
      public CodecAllocatorClient {
 public:
  static VideoDecodeAccelerator::Capabilities GetCapabilities(
      const gpu::GpuPreferences& gpu_preferences);

  AndroidVideoDecodeAccelerator(
      CodecAllocator* codec_allocator,
      std::unique_ptr<AndroidVideoSurfaceChooser> surface_chooser,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const GetContextGroupCallback& get_context_group_cb,
      const AndroidOverlayMojoFactoryCB& overlay_factory_cb,
      DeviceInfo* device_info);

  ~AndroidVideoDecodeAccelerator() override;

  // VideoDecodeAccelerator implementation:
  bool Initialize(const Config& config, Client* client) override;
  void Decode(const BitstreamBuffer& bitstream_buffer) override;
  void AssignPictureBuffers(const std::vector<PictureBuffer>& buffers) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  void Flush() override;
  void Reset() override;
  void SetOverlayInfo(const OverlayInfo& overlay_info) override;
  void Destroy() override;
  bool TryToSetupDecodeOnSeparateThread(
      const base::WeakPtr<Client>& decode_client,
      const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner)
      override;

  // AVDAStateProvider implementation:
  const gfx::Size& GetSize() const override;
  gpu::gles2::ContextGroup* GetContextGroup() const override;
  // Notifies the client about the error and sets |state_| to |ERROR|.  If we're
  // in the middle of Initialize, we guarantee that Initialize will return
  // failure.  If deferred init is pending, then we'll fail deferred init.
  // Otherwise, we'll signal errors normally.
  void NotifyError(Error error) override;
  PromotionHintAggregator::NotifyPromotionHintCB GetPromotionHintCB() override;

  // CodecAllocatorClient implementation:
  void OnCodecConfigured(
      std::unique_ptr<MediaCodecBridge> media_codec,
      scoped_refptr<AVDASurfaceBundle> surface_bundle) override;

 private:
  friend class AVDAManager;

  // TODO(timav): evaluate the need for more states in the AVDA state machine.
  enum State {
    NO_ERROR,
    ERROR,
    // We haven't initialized |surface_chooser_| yet, so we don't have a surface
    // or a codec.  After we initialize |surface_chooser_|, we'll transition to
    // WAITING_FOR_CODEC, NO_ERROR, or ERROR.
    BEFORE_OVERLAY_INIT,
    // Set when we are asynchronously constructing the codec.  Will transition
    // to NO_ERROR or ERROR depending on success.
    WAITING_FOR_CODEC,
    // Set when we have a codec, but it doesn't yet have a key.
    WAITING_FOR_KEY,
    // The output surface was destroyed. We must not configure a new MediaCodec
    // with the destroyed surface.
    SURFACE_DESTROYED,
  };

  enum DrainType {
    DRAIN_FOR_FLUSH,
    DRAIN_FOR_RESET,
    DRAIN_FOR_DESTROY,
  };

  // Called once before (possibly deferred) initialization succeeds, to set up
  // |surface_chooser_| with our initial factory from VDA::Config.
  void StartSurfaceChooser();

  // Start a transition to an overlay, or, if |!overlay|, TextureOwner.  The
  // transition doesn't have to be immediate; we'll favor not dropping frames.
  void OnSurfaceTransition(std::unique_ptr<AndroidOverlay> overlay);

  // Called by AndroidOverlay when a surface is lost.  We will discard pending
  // frames, as needed, to switch away from |overlay| if we're using it.  Before
  // we return, we will have either dropped |overlay| if we own it, or posted
  // it for async release with the codec that's using it.  We also handle the
  // case where we're not using |overlay| at all, since that can happen too
  // while async codec release is pending.
  void OnStopUsingOverlayImmediately(AndroidOverlay* overlay);

  // Initializes the picture buffer manager to use the current surface, once
  // it is available.  This is not normally called directly, but rather via
  // StartSurfaceCreation.  If we have a media codec already, then this will
  // attempt to setSurface the new surface.  Otherwise, it will start codec
  // config using the new surface.  In that case, there might not be a codec
  // ready even if this succeeds, but async config will be started.  If
  // setSurface fails, this will not replace the codec.  On failure, this will
  // transition |state_| to ERROR.
  // Note that this assumes that there is an |incoming_bundle_| that we'll use.
  // On success, we'll replace the bundle in |codec_config_|.  On failure, we'll
  // delete the incoming bundle.
  void InitializePictureBufferManager();

  // A part of destruction process that is sometimes postponed after the drain.
  void ActualDestroy();

  // Configures |media_codec_| with the given codec parameters from the client.
  // This configuration will (probably) not be complete before this call
  // returns.  Multiple calls before completion will be ignored.  |state_|
  // must be NO_ERROR or WAITING_FOR_CODEC.  Note that, once you call this,
  // you should be careful to avoid modifying members of |codec_config_| until
  // |state_| is no longer WAITING_FOR_CODEC.
  void ConfigureMediaCodecAsynchronously();

  // Like ConfigureMediaCodecAsynchronously, but synchronous.  Will NotifyError
  // on failure.  Since all configuration is done synchronously, there is no
  // concern with modifying |codec_config_| after this returns.
  void ConfigureMediaCodecSynchronously();

  // Sends the decoded frame specified by |codec_buffer_index| to the client.
  void SendDecodedFrameToClient(int32_t codec_buffer_index,
                                int32_t bitstream_id);

  // Does pending IO tasks if any. Once this is called, it polls |media_codec_|
  // until it finishes pending tasks. For the polling, |kDecodePollDelay| is
  // used.
  void DoIOTask(bool start_timer);

  // Feeds buffers in |pending_bitstream_records_| to |media_codec_|. Returns
  // true if one was queued.
  bool QueueInput();

  // Dequeues output from |media_codec_| and feeds the decoded frame to the
  // client.  Returns a hint about whether calling again might produce
  // more output.
  bool DequeueOutput();

  // Requests picture buffers from the client.
  void RequestPictureBuffers();

  // Decode the content in the |bitstream_buffer|. Note that a
  // |bitstream_buffer| of id as -1 indicates a flush command.
  void DecodeBuffer(const BitstreamBuffer& bitstream_buffer);

  // Called during Initialize() for encrypted streams to set up the CDM.
  void InitializeCdm();

  // Called after the CDM obtains a MediaCrypto object.
  void OnMediaCryptoReady(JavaObjectPtr media_crypto,
                          bool requires_secure_video_codec);

  // Called when a new key is added to the CDM.
  void OnKeyAdded();

  // Notifies the client that deferred initialization succeeded.  If it fails,
  // then call NotifyError instead.
  void NotifyInitializationSucceeded();

  // Notifies the client about the availability of a picture.
  void NotifyPictureReady(const Picture& picture);

  // Notifies the client that the input buffer identifed by input_buffer_id has
  // been processed.
  void NotifyEndOfBitstreamBuffer(int input_buffer_id);

  // Notifies the client that the decoder was flushed.
  void NotifyFlushDone();

  // Notifies the client that the decoder was reset.
  void NotifyResetDone();

  // Start or stop our work-polling timer based on whether we did any work, and
  // how long it has been since we've done work.  Calling this with true will
  // start the timer.  Calling it with false may stop the timer.
  void ManageTimer(bool did_work);

  // Start the MediaCodec drain process by adding end_of_stream() buffer to the
  // encoded buffers queue. When we receive EOS from the output buffer the drain
  // process completes and we perform the action depending on the |drain_type|.
  void StartCodecDrain(DrainType drain_type);

  // Returns true if we are currently draining the codec and doing that as part
  // of Reset() or Destroy() VP8 workaround. (http://crbug.com/598963). We won't
  // display any frames and disable normal errors handling.
  bool IsDrainingForResetOrDestroy() const;

  // A helper method that performs the operation required after the drain
  // completion (usually when we receive EOS in the output). The operation
  // itself depends on the |drain_type_|.
  void OnDrainCompleted();

  // Resets MediaCodec and buffers/containers used for storing output. These
  // components need to be reset upon EOS to decode a later stream. Input state
  // (e.g. queued BitstreamBuffers) is not reset, as input following an EOS
  // is still valid and should be processed.
  void ResetCodecState();

  // Indicates if MediaCodec should not be used for software decoding since we
  // have safer versions elsewhere.
  bool IsMediaCodecSoftwareDecodingForbidden() const;

  // On platforms which support seamless surface changes, this will reinitialize
  // the picture buffer manager with the new surface. This function reads and
  // clears the surface id from |pending_surface_id_|. It will issue a decode
  // error if the surface change fails. Returns false on failure.
  bool UpdateSurface();

  // Release |media_codec_| if it's not null, and notify
  // |picture_buffer_manager_|.
  void ReleaseCodec();

  // ReleaseCodec(), and also drop our ref to it's surface bundle.  This is
  // the right thing to do unless you're planning to re-use the bundle with
  // another codec.  Normally, one doesn't.
  void ReleaseCodecAndBundle();

  // Send a |hint| to |promotion_hint_aggregator_|.
  void NotifyPromotionHint(PromotionHintAggregator::Hint hint);

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // To expose client callbacks from VideoDecodeAccelerator.
  Client* client_;

  CodecAllocator* codec_allocator_;

  // Callback to set the correct gl context.
  MakeGLContextCurrentCallback make_context_current_cb_;

  // Callback to get the ContextGroup*.
  GetContextGroupCallback get_context_group_cb_;

  // The current state of this class. For now, this is used only for setting
  // error state.
  State state_;

  // The assigned picture buffers by picture buffer id.
  AVDAPictureBufferManager::PictureBufferMap output_picture_buffers_;

  // This keeps the free picture buffer ids which can be used for sending
  // decoded frames to the client.
  base::queue<int32_t> free_picture_ids_;

  // The low-level decoder which Android SDK provides.
  std::unique_ptr<MediaCodecBridge> media_codec_;

  // Set to true after requesting picture buffers to the client.
  bool picturebuffers_requested_;

  // The resolution of the stream.
  gfx::Size size_;

  // Handy structure to remember a BitstreamBuffer and also its shared memory,
  // if any.  The goal is to prevent leaving a BitstreamBuffer's shared memory
  // handle open.
  struct BitstreamRecord {
    BitstreamRecord(const BitstreamBuffer&);
    BitstreamRecord(BitstreamRecord&& other);
    ~BitstreamRecord();

    BitstreamBuffer buffer;

    // |memory| may be null if buffer has no data.
    std::unique_ptr<UnalignedSharedMemory> memory;
  };

  // Encoded bitstream buffers to be passed to media codec, queued until an
  // input buffer is available.
  base::queue<BitstreamRecord> pending_bitstream_records_;

  // A map of presentation timestamp to bitstream buffer id for the bitstream
  // buffers that have been submitted to the decoder but haven't yet produced an
  // output frame with the same timestamp. Note: there will only be one entry
  // for multiple bitstream buffers that have the same presentation timestamp.
  std::map<base::TimeDelta, int32_t> bitstream_buffers_in_decoder_;

  // Keeps track of bitstream ids notified to the client with
  // NotifyEndOfBitstreamBuffer() before getting output from the bitstream.
  std::list<int32_t> bitstreams_notified_in_advance_;

  AVDAPictureBufferManager picture_buffer_manager_;

  // Time at which we last did useful work on io_timer_.
  base::TimeTicks most_recent_work_;

  // The ongoing drain operation, if any.
  base::Optional<DrainType> drain_type_;

  // Holds a ref-count to the CDM to avoid using the CDM after it's destroyed.
  scoped_refptr<ContentDecryptionModule> cdm_for_reference_holding_only_;

  // Owned by CDM which is external to this decoder.
  MediaCryptoContext* media_crypto_context_;

  // MediaDrmBridge requires registration/unregistration of the player, this
  // registration id is used for this.
  int cdm_registration_id_;

  // Configuration that we use for MediaCodec.
  // Do not update any of its members while |state_| is WAITING_FOR_CODEC.
  scoped_refptr<CodecConfig> codec_config_;

  // Index of the dequeued and filled buffer that we keep trying to enqueue.
  // Such buffer appears in MEDIA_CODEC_NO_KEY processing.
  int pending_input_buf_index_;

  // Monotonically increasing value that is used to prevent old, delayed errors
  // from being sent after a reset.
  int error_sequence_token_;

  // Are we currently processing a call to Initialize()?  Please don't use this
  // unless you're NotifyError.
  bool during_initialize_;

  // True if and only if VDA initialization is deferred, and we have not yet
  // called NotifyInitializationComplete.
  bool deferred_initialization_pending_;

  // Indicates if ResetCodecState() should be called upon the next call to
  // Decode(). Allows us to avoid trashing the last few frames of a playback
  // when the EOS buffer is received.
  bool codec_needs_reset_;

  // True if surface creation and |picture_buffer_manager_| initialization has
  // been defered until the first Decode() call.
  bool defer_surface_creation_;

  // Copy of the VDA::Config we were given.
  Config config_;

  // SurfaceBundle that we're going to use for StartSurfaceCreation.  This is
  // separate than the bundle in |codec_config_|, since we can start surface
  // creation while another codec is using the old surface.  For example, if
  // we're going to SetSurface, then the current codec will depend on the
  // current bundle until then.
  scoped_refptr<AVDASurfaceBundle> incoming_bundle_;

  // If we have been given an overlay to use, then this is it.  If we've been
  // told to move to TextureOwner, then this will be value() == nullptr.
  base::Optional<std::unique_ptr<AndroidOverlay>> incoming_overlay_;

  SurfaceChooserHelper surface_chooser_helper_;

  DeviceInfo* device_info_;

  bool force_defer_surface_creation_for_testing_;

  bool force_allow_software_decoding_for_testing_;

  // Optional factory to produce mojo AndroidOverlay instances.
  AndroidOverlayMojoFactoryCB overlay_factory_cb_;

  std::unique_ptr<PromotionHintAggregator> promotion_hint_aggregator_;

  // Update |cached_frame_information_|.
  void CacheFrameInformation();

  // Most recently cached frame information, so that we can dispatch it without
  // recomputing it on every frame.  It changes very rarely.
  SurfaceChooserHelper::FrameInformation cached_frame_information_ =
      SurfaceChooserHelper::FrameInformation::NON_OVERLAY_INSECURE;

  // WeakPtrFactory for posting tasks back to |this|.
  base::WeakPtrFactory<AndroidVideoDecodeAccelerator> weak_this_factory_;

  friend class AndroidVideoDecodeAcceleratorTest;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_ANDROID_VIDEO_DECODE_ACCELERATOR_H_
