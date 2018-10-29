// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_CODEC_ALLOCATOR_H_
#define MEDIA_GPU_ANDROID_CODEC_ALLOCATOR_H_

#include <stddef.h>

#include <memory>

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/time/tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "media/base/android/android_overlay.h"
#include "media/base/android/media_codec_bridge_impl.h"
#include "media/base/android/media_crypto_context.h"
#include "media/base/media.h"
#include "media/base/video_codecs.h"
#include "media/gpu/android/avda_surface_bundle.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace media {

// For TaskRunnerFor. These are used as vector indices, so please update
// CodecAllocator's constructor if you add / change them.
enum TaskType {
  // Task for an autodetected MediaCodec instance.
  AUTO_CODEC = 0,

  // Task for a software-codec-required MediaCodec.
  SW_CODEC = 1,
};

// Configuration info for MediaCodec.
// This is used to shuttle configuration info between threads without needing
// to worry about the lifetime of the AVDA instance.
class MEDIA_GPU_EXPORT CodecConfig
    : public base::RefCountedThreadSafe<CodecConfig> {
 public:
  CodecConfig();

  VideoCodec codec = kUnknownVideoCodec;

  // The surface that MediaCodec is configured to output to.
  scoped_refptr<AVDASurfaceBundle> surface_bundle;

  // The MediaCrypto that MediaCodec is configured with for an encrypted stream.
  JavaObjectPtr media_crypto;

  // Whether MediaCrypto requires a secure codec.
  bool requires_secure_codec = false;

  // The initial coded size. The actual size might change at any time, so this
  // is only a hint.
  gfx::Size initial_expected_coded_size;

  // Whether creating a software decoder backed MediaCodec is forbidden.
  bool software_codec_forbidden = false;

  // Codec specific data (SPS and PPS for H264).
  std::vector<uint8_t> csd0;
  std::vector<uint8_t> csd1;

  // VP9 HDR metadata is only embedded in the container
  // HDR10 meta data is embedded in the video stream
  VideoColorSpace container_color_space;
  base::Optional<HDRMetadata> hdr_metadata;

  base::RepeatingClosure on_buffers_available_cb;

 protected:
  friend class base::RefCountedThreadSafe<CodecConfig>;
  virtual ~CodecConfig();

 private:
  DISALLOW_COPY_AND_ASSIGN(CodecConfig);
};

class AVDASurfaceAllocatorClient {
 public:
  // Called when the requested SurfaceView becomes available after a call to
  // AllocateSurface()
  virtual void OnSurfaceAvailable(bool success) = 0;

  // Called when the allocated surface is being destroyed. This must either
  // replace the surface with MediaCodec#setSurface, or release the MediaCodec
  // it's attached to. The client no longer owns the surface and doesn't
  // need to call DeallocateSurface();
  virtual void OnSurfaceDestroyed() = 0;

 protected:
  ~AVDASurfaceAllocatorClient() {}
};

class CodecAllocatorClient {
 public:
  // Called on the main thread when a new MediaCodec is configured.
  // |media_codec| will be null if configuration failed.
  virtual void OnCodecConfigured(
      std::unique_ptr<MediaCodecBridge> media_codec,
      scoped_refptr<AVDASurfaceBundle> surface_bundle) = 0;

 protected:
  ~CodecAllocatorClient() {}
};

// CodecAllocator manages threads for allocating and releasing MediaCodec
// instances.  These activities can hang, depending on android version, due
// to mediaserver bugs.  CodecAllocator detects these cases, and reports
// on them to allow software fallback if the HW path is hung up.
class MEDIA_GPU_EXPORT CodecAllocator {
 public:
  static CodecAllocator* GetInstance(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  using CodecFactoryCB =
      base::RepeatingCallback<std::unique_ptr<MediaCodecBridge>(
          VideoCodec codec,
          CodecType codec_type,
          const gfx::Size& size,  // Output frame size.
          const base::android::JavaRef<jobject>& surface,
          const base::android::JavaRef<jobject>& media_crypto,
          const std::vector<uint8_t>& csd0,
          const std::vector<uint8_t>& csd1,
          const VideoColorSpace& color_space,
          const base::Optional<HDRMetadata>& hdr_metadata,
          bool allow_adaptive_playback,
          base::RepeatingClosure on_buffers_available_cb)>;

  // Make sure the construction threads are started for |client|.  If the
  // threads fail to start, then codec allocation may fail.
  virtual void StartThread(CodecAllocatorClient* client);
  virtual void StopThread(CodecAllocatorClient* client);

  // Create and configure a MediaCodec synchronously.
  virtual std::unique_ptr<MediaCodecBridge> CreateMediaCodecSync(
      scoped_refptr<CodecConfig> codec_config);

  // Create and configure a MediaCodec asynchronously. The result is delivered
  // via OnCodecConfigured().
  virtual void CreateMediaCodecAsync(base::WeakPtr<CodecAllocatorClient> client,
                                     scoped_refptr<CodecConfig> codec_config);

  // Asynchronously release |media_codec| with the attached surface.  We will
  // drop our reference to |surface_bundle| on the main thread after the codec
  // is deallocated, since the codec isn't using it anymore.  We will not take
  // other action on it (e.g., calling ReleaseSurfaceTexture if it has one),
  // since some other codec might be going to use it.  We just want to be sure
  // that it outlives |media_codec|.
  virtual void ReleaseMediaCodec(
      std::unique_ptr<MediaCodecBridge> media_codec,
      scoped_refptr<AVDASurfaceBundle> surface_bundle);

  // Return true if and only if there is any AVDA registered.
  bool IsAnyRegisteredAVDA();

  // Return a reference to the thread for unit tests.
  base::Thread& GetThreadForTesting(TaskType task_type);

  // Wait for a bounded amount of time for |overlay| to be freed, if it's
  // in use pending release of a codec.  Returns true on success, or false if
  // the wait times out.
  bool WaitForPendingReleaseForTesting(AndroidOverlay* overlay);

 protected:
  // |tick_clock| and |stop_event| are for tests only.
  CodecAllocator(CodecAllocator::CodecFactoryCB factory_cb,
                 scoped_refptr<base::SequencedTaskRunner> task_runner,
                 const base::TickClock* tick_clock = nullptr,
                 base::WaitableEvent* stop_event = nullptr);
  virtual ~CodecAllocator();

  // Struct to own a codec and surface bundle, with a custom deleter to post
  // destruction to the right thread.
  struct MediaCodecAndSurface {
    MediaCodecAndSurface(std::unique_ptr<MediaCodecBridge> media_codec,
                         scoped_refptr<AVDASurfaceBundle> surface_bundle);
    ~MediaCodecAndSurface();
    std::unique_ptr<MediaCodecBridge> media_codec;
    scoped_refptr<AVDASurfaceBundle> surface_bundle;
  };

  // Forward |media_codec|, which is configured to output to |surface_bundle|,
  // to |client| if |client| is still around.  Otherwise, release the codec and
  // then drop our ref to |surface_bundle|.  This is called on |task_runner_|.
  // It may only reference |client| from |client_task_runner|.
  void ForwardOrDropCodec(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      base::WeakPtr<CodecAllocatorClient> client,
      TaskType task_type,
      scoped_refptr<AVDASurfaceBundle> surface_bundle,
      std::unique_ptr<MediaCodecBridge> media_codec);

  // Forward |surface_bundle| and |media_codec| to |client| on the right thread
  // to access |client|.
  void ForwardOrDropCodecOnClientThread(
      base::WeakPtr<CodecAllocatorClient> client,
      std::unique_ptr<MediaCodecAndSurface> codec_and_surface);

 private:
  friend class CodecAllocatorTest;

  struct OwnerRecord {
    AVDASurfaceAllocatorClient* owner = nullptr;
    AVDASurfaceAllocatorClient* waiter = nullptr;
  };

  class HangDetector : public base::MessageLoop::TaskObserver {
   public:
    HangDetector(const base::TickClock* tick_clock);
    void WillProcessTask(const base::PendingTask& pending_task) override;
    void DidProcessTask(const base::PendingTask& pending_task) override;
    bool IsThreadLikelyHung();

   private:
    base::Lock lock_;

    // Non-null when a task is currently running.
    base::TimeTicks task_start_time_;

    const base::TickClock* tick_clock_;

    DISALLOW_COPY_AND_ASSIGN(HangDetector);
  };

  // Handy combination of a thread and hang detector for it.
  struct ThreadAndHangDetector {
    ThreadAndHangDetector(const std::string& name,
                          const base::TickClock* tick_clock)
        : thread(name), hang_detector(tick_clock) {}
    base::Thread thread;
    HangDetector hang_detector;
  };

  // Helper function for CreateMediaCodecAsync which takes the task runner on
  // which it should post the reply to |client|.
  void CreateMediaCodecAsyncInternal(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      base::WeakPtr<CodecAllocatorClient> client,
      scoped_refptr<CodecConfig> codec_config);

  // Return the task type to use for a new codec allocation, or nullopt if
  // both threads are hung.
  base::Optional<TaskType> TaskTypeForAllocation(bool software_codec_forbidden);

  // Return the task runner for tasks of type |type|.
  scoped_refptr<base::SingleThreadTaskRunner> TaskRunnerFor(TaskType task_type);

  // Called on the gpu main thread when a codec is freed on a codec thread.
  // |surface_bundle| is the surface bundle that the codec was using. It's
  // important to pass this through to ensure a) it outlives the codec, and b)
  // it's deleted on the right thread.
  void OnMediaCodecReleased(scoped_refptr<AVDASurfaceBundle> surface_bundle);

  // Stop the thread indicated by |index|. This signals stop_event_for_testing_
  // after both threads are stopped.
  void StopThreadTask(size_t index);

  // Task runner on which we do all our work.  All members should be accessed
  // only from this task runner.  |task_runner_| itself may be referenced from
  // any thread (hence const).
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // All registered AVDAs.
  std::set<CodecAllocatorClient*> clients_;

  // Waitable events for ongoing release tasks indexed by overlay so we can
  // wait on the codec release if the surface attached to it is being destroyed.
  // This really is needed only for ContentVideoViewOverlay, since it requires
  // synchronous releases with respect to the main thread.
  std::map<AndroidOverlay*, base::WaitableEvent> pending_codec_releases_;

  // Threads for each of TaskType.  They are started / stopped as avda instances
  // show and and request them.  The vector indicies must match TaskType.
  std::vector<ThreadAndHangDetector*> threads_;

  base::WaitableEvent* stop_event_for_testing_;

  // Saves the TaskType used to create a given codec so it can later be released
  // on the same thread.
  std::map<MediaCodecBridge*, TaskType> codec_task_types_;

  // Low-level codec factory, for testing.
  CodecFactoryCB factory_cb_;

  // For canceling pending StopThreadTask()s.
  base::WeakPtrFactory<CodecAllocator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(CodecAllocator);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_CODEC_ALLOCATOR_H_
