// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECOMPRESSION_INTERFACE_H_
#define MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECOMPRESSION_INTERFACE_H_

#include <CoreMedia/CoreMedia.h>
#include <VideoToolbox/VideoToolbox.h>

#include <utility>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"

namespace media {

// Manages reconfiguration of decompression sessions and hides the threading.
// Callbacks are never called re-entrantly or after destruction.
class VideoToolboxDecompressionInterface {
 public:
  using OutputCB =
      base::RepeatingCallback<void(base::ScopedCFTypeRef<CVImageBufferRef>,
                                   void*)>;
  using ErrorCB = base::OnceClosure;

  VideoToolboxDecompressionInterface(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      OutputCB output_cb,
      ErrorCB error_cb);

  ~VideoToolboxDecompressionInterface();

  // Decode |sample|, tagged with |context|.
  void Decode(base::ScopedCFTypeRef<CMSampleBufferRef> sample, void* context);

  // Discards decodes that have not been output yet.
  void Reset();

  // The number of decodes that have not been output yet.
  size_t PendingDecodes();

  // Called by OnOutputThunk().
  void OnOutputOnAnyThread(void* context,
                           OSStatus status,
                           VTDecodeInfoFlags info_flags,
                           base::ScopedCFTypeRef<CVImageBufferRef> image);

 private:
  // Shut down and call |error_cb_|.
  void NotifyError();

  // Helper to call |error_cb|. Used to post |error_cb_| without calling it
  // after destruction.
  void CallErrorCB(ErrorCB error_cb);

  // Send queued decodes to VideoToolbox if possible.
  [[nodiscard]] bool ProcessDecodes();

  // Create a new VideoToolbox decompression session for |format|.
  [[nodiscard]] bool CreateSession(CMFormatDescriptionRef format);

  // Shut down the active session, synchronously.
  void DestroySession();

  // Called by OnOutputOnAnyThread().
  void OnOutput(void* context,
                OSStatus status,
                VTDecodeInfoFlags info_flags,
                base::ScopedCFTypeRef<CVImageBufferRef> image);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  OutputCB output_cb_;

  // |!error_cb_| indicates that there has been an error.
  ErrorCB error_cb_;

  // Decodes that have not been sent to VideoToolbox.
  base::queue<std::pair<base::ScopedCFTypeRef<CMSampleBufferRef>, void*>>
      pending_decodes_;

  base::ScopedCFTypeRef<VTDecompressionSessionRef> active_session_;
  base::ScopedCFTypeRef<CMFormatDescriptionRef> active_format_;

  // TODO(crbug.com/1331597): Check if it is efficient to query
  // kVTDecompressionPropertyKey_NumberOfFramesBeingDecoded instead.
  int active_decodes_ = 0;

  // Destroy the active session once it becomes empty. Used to prepare for
  // format changes.
  bool draining_ = false;

  // Used in OnOutputOnAnyThread() to hop to |task_runner_|.
  base::WeakPtr<VideoToolboxDecompressionInterface> weak_this_;
  base::WeakPtrFactory<VideoToolboxDecompressionInterface> weak_this_factory_{
      this};
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECOMPRESSION_INTERFACE_H_
