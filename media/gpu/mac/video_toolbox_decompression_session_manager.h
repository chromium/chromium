// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECOMPRESSION_SESSION_MANAGER_H_
#define MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECOMPRESSION_SESSION_MANAGER_H_

#include <CoreMedia/CoreMedia.h>
#include <VideoToolbox/VideoToolbox.h>

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/apple/scoped_cftyperef.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/decoder_status.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

class MediaLog;
struct VideoToolboxDecodeMetadata;
class VideoToolboxDecompressionSession;
struct VideoToolboxDecompressionSessionMetadata;

// Handles creating VideoToolboxDecompressionSessions when reconfiguration is
// required. Callbacks are never called re-entrantly or after destruction.
class MEDIA_GPU_EXPORT VideoToolboxDecompressionSessionManager {
 public:
  using OutputCB = base::RepeatingCallback<void(
      base::apple::ScopedCFTypeRef<CVImageBufferRef>,
      std::unique_ptr<VideoToolboxDecodeMetadata> metadata)>;
  using ErrorCB = base::OnceCallback<void(DecoderStatus)>;

  VideoToolboxDecompressionSessionManager(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<MediaLog> media_log,
      OutputCB output_cb,
      ErrorCB error_cb);

  ~VideoToolboxDecompressionSessionManager();

  // Decode |sample|, tagged with |context|.
  void Decode(base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample,
              std::unique_ptr<VideoToolboxDecodeMetadata> metadata);

  // Discards decodes that have not been output yet.
  void Reset();

  // The number of decodes that have not been output yet.
  size_t NumDecodes();

  // Public for testing.
  void SetDecompressionSessionForTesting(
      std::unique_ptr<VideoToolboxDecompressionSession> decompression_session);

  // Public for testing.
  void OnOutput(uintptr_t context,
                OSStatus status,
                VTDecodeInfoFlags flags,
                base::apple::ScopedCFTypeRef<CVImageBufferRef> image);

 private:
  // Shut down and call |error_cb_|.
  void NotifyError(DecoderStatus status);

  // Helper to call |error_cb|. Used to post |error_cb_| without calling it
  // after destruction.
  void CallErrorCB(ErrorCB error_cb, DecoderStatus status);

  // Send queued decodes to VideoToolbox if possible.
  [[nodiscard]] bool Process();

  // Create a new VideoToolbox decompression session for |format|.
  [[nodiscard]] bool CreateSession(
      CMFormatDescriptionRef format,
      const VideoToolboxDecompressionSessionMetadata& session_metadata);

  // Shut down the active session, synchronously.
  void DestroySession();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<MediaLog> media_log_;
  OutputCB output_cb_;
  ErrorCB error_cb_;

  bool has_error_ = false;

  // Decodes that have not been sent to VideoToolbox yet.
  base::queue<std::pair<base::apple::ScopedCFTypeRef<CMSampleBufferRef>,
                        std::unique_ptr<VideoToolboxDecodeMetadata>>>
      pending_decodes_;

  std::unique_ptr<VideoToolboxDecompressionSession> decompression_session_;

  // The last used format description, used to check if the current format
  // description is potentially changed.
  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> active_format_;

  // Pointers to decode metadata are passed to VideoToolbox as decode context,
  // but returned pointers are always looked up in this table rather than
  // dereferenced. If VideoToolbox were to output after a session is destroyed,
  // or output the same context twice, a decode error would result.
  base::flat_map<uintptr_t, std::unique_ptr<VideoToolboxDecodeMetadata>>
      active_decodes_;

  // Destroy the active session once it becomes empty. Used to prepare for
  // format changes.
  bool draining_ = false;

  base::WeakPtr<VideoToolboxDecompressionSessionManager> weak_this_;
  base::WeakPtrFactory<VideoToolboxDecompressionSessionManager> weak_this_factory_{
      this};
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VIDEO_TOOLBOX_DECOMPRESSION_SESSION_MANAGER_H_
