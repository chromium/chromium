// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECOMPRESSION_SESSION_H_
#define MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECOMPRESSION_SESSION_H_

#include <CoreMedia/CoreMedia.h>
#include <VideoToolbox/VideoToolbox.h>

#include <stdint.h>
#include <memory>

#include "base/apple/scoped_cftyperef.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

class MediaLog;

// This interface wraps VideoToolbox platform APIs so that they can be swapped
// out for testing.
class MEDIA_GPU_EXPORT VideoToolboxDecompressionSession {
 protected:
  VideoToolboxDecompressionSession() = default;

 public:
  virtual ~VideoToolboxDecompressionSession() = default;

  virtual bool Create(CMFormatDescriptionRef format,
                      CFDictionaryRef decoder_config,
                      CFDictionaryRef image_config) = 0;
  virtual void Invalidate() = 0;
  virtual bool IsValid() = 0;
  virtual bool CanAcceptFormat(CMFormatDescriptionRef format) = 0;
  virtual bool DecodeFrame(CMSampleBufferRef sample, uintptr_t context) = 0;
};

// Standard implementation of VideoToolboxDecompressionSession. It's not quite
// trivial, since it does handle hopping outputs (which arrive on any thread) to
// |task_runner|.
class MEDIA_GPU_EXPORT VideoToolboxDecompressionSessionImpl
    : public VideoToolboxDecompressionSession {
 public:
  using OutputCB = base::RepeatingCallback<void(
      uintptr_t,
      OSStatus,
      VTDecodeInfoFlags,
      base::apple::ScopedCFTypeRef<CVImageBufferRef>)>;

  VideoToolboxDecompressionSessionImpl(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<MediaLog> media_log,
      OutputCB output_cb);
  ~VideoToolboxDecompressionSessionImpl() override;

  // VideoToolboxDecompressionSession implementation.
  bool Create(CMFormatDescriptionRef format,
              CFDictionaryRef decoder_config,
              CFDictionaryRef image_config) override;
  void Invalidate() override;
  bool IsValid() override;
  bool CanAcceptFormat(CMFormatDescriptionRef format) override;
  bool DecodeFrame(CMSampleBufferRef sample, uintptr_t context) override;

  // Called by OnOutputThunk().
  void OnOutputOnAnyThread(
      uintptr_t context,
      OSStatus status,
      VTDecodeInfoFlags flags,
      base::apple::ScopedCFTypeRef<CVImageBufferRef> image);

 private:
  void OnOutput(uintptr_t context,
                OSStatus status,
                VTDecodeInfoFlags flags,
                base::apple::ScopedCFTypeRef<CVImageBufferRef> image);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<MediaLog> media_log_;
  OutputCB output_cb_;

  base::apple::ScopedCFTypeRef<VTDecompressionSessionRef> session_;

  // Used in OnOutputOnAnyThread() to hop to |task_runner_|.
  base::WeakPtr<VideoToolboxDecompressionSessionImpl> weak_this_;
  base::WeakPtrFactory<VideoToolboxDecompressionSessionImpl> weak_this_factory_{
      this};
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECOMPRESSION_SESSION_H_
