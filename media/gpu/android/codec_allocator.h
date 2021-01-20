// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_CODEC_ALLOCATOR_H_
#define MEDIA_GPU_ANDROID_CODEC_ALLOCATOR_H_

#include <stddef.h>

#include <memory>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/no_destructor.h"
#include "base/sequenced_task_runner.h"
#include "media/base/android/android_util.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_codec_bridge_impl.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace base {
class TickClock;
}

namespace media {

// CodecAllocator manages allocating and releasing MediaCodec instances. These
// activities can hang, depending on android version, due to mediaserver bugs.
// CodecAllocator detects these cases, and allows software fallback if the HW
// path is hung up.
class MEDIA_GPU_EXPORT CodecAllocator {
 public:
  static CodecAllocator* GetInstance(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  using CodecFactoryCB =
      base::RepeatingCallback<std::unique_ptr<MediaCodecBridge>(
          const VideoCodecConfig& config)>;

  // Create and configure a MediaCodec asynchronously. The result is delivered
  // via OnCodecCreated(). Will modify VideoCodecConfig::codec_type (but no
  // other field) as needed if hardware codecs can't currently be created.
  using CodecCreatedCB =
      base::OnceCallback<void(std::unique_ptr<MediaCodecBridge>)>;
  virtual void CreateMediaCodecAsync(
      CodecCreatedCB codec_created_cb,
      std::unique_ptr<VideoCodecConfig> codec_config);

  // Asynchronously release |codec|, |codec_released_cb| will be called after
  // the release has been completed.
  virtual void ReleaseMediaCodec(std::unique_ptr<MediaCodecBridge> codec,
                                 base::OnceClosure codec_released_cb);

 protected:
  friend class base::NoDestructor<CodecAllocator>;

  CodecAllocator(CodecFactoryCB factory_cb,
                 scoped_refptr<base::SequencedTaskRunner> task_runner);
  virtual ~CodecAllocator();

 private:
  friend class CodecAllocatorTest;

  // Called on |task_runner_| after the codec has been created.
  void OnCodecCreated(base::TimeTicks start_time,
                      CodecCreatedCB codec_created_cb,
                      std::unique_ptr<MediaCodecBridge> codec);

  // Called on |task_runner_| after a codec is freed.
  void OnCodecReleased(base::TimeTicks start_time,
                       base::OnceClosure codec_released_cb);

  // Indicates if we have likely hung |primary_task_runner_| and should fall
  // back to the secondary task runner for software only codecs.
  bool IsPrimaryTaskRunnerLikelyHung() const;

  // Returns either |primary_task_runner_| or |secondary_task_runner_| depending
  // on if |primary_task_runner_| is hung. Sets or clears |force_sw_codecs_|
  // based on that same information.
  base::SequencedTaskRunner* SelectCodecTaskRunner();

  // Erases the first entry for |start_time| in |pending_operations_|.
  void CompletePendingOperation(base::TimeTicks start_time);

  // Task runner on which we do all our work. All members should be accessed
  // only from this task runner. |task_runner_| itself may be referenced from
  // any thread (hence const).
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Low-level codec factory.
  const CodecFactoryCB factory_cb_;

  // Tick clock which can be replaced for test purposes.
  const base::TickClock* tick_clock_;

  // The two task runners used for codec operations. The primary is allowed to
  // create CodecType::kAny MediaCodec instances, while the secondary one is
  // only allowed to create CodecType::kSoftware instances.
  scoped_refptr<base::SequencedTaskRunner> primary_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> secondary_task_runner_;

  base::circular_deque<base::TimeTicks> pending_operations_;

  // True if only software codec creation is currently allowed due to hangs.
  bool force_sw_codecs_ = false;

  DISALLOW_COPY_AND_ASSIGN(CodecAllocator);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_CODEC_ALLOCATOR_H_
