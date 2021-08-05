// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_APPLY_CONSTRAINTS_PROCESSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_APPLY_CONSTRAINTS_PROCESSOR_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink-forward.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/apply_constraints_request.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
class MediaStreamAudioSource;
class MediaStreamVideoTrack;

// ApplyConstraintsProcessor is responsible for processing applyConstraints()
// requests. Only one applyConstraints() request can be processed at a time.
// ApplyConstraintsProcessor must be created, called and destroyed on the main
// render thread. There should be only one ApplyConstraintsProcessor per frame.
class MODULES_EXPORT ApplyConstraintsProcessor final
    : public GarbageCollected<ApplyConstraintsProcessor> {
 public:
  using MediaDevicesDispatcherCallback = base::RepeatingCallback<
      blink::mojom::blink::MediaDevicesDispatcherHost*()>;
  ApplyConstraintsProcessor(
      MediaDevicesDispatcherCallback media_devices_dispatcher_cb,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~ApplyConstraintsProcessor();

  // Starts processing of |request|. When processing of |request| is complete,
  // it notifies by invoking |callback|.
  // This method must be called only if there is no request currently being
  // processed.
  void ProcessRequest(blink::ApplyConstraintsRequest* request,
                      base::OnceClosure callback);

  void Trace(Visitor* visitor) const { visitor->Trace(current_request_); }

 private:
  // Helpers for video device-capture requests.
  void ProcessVideoDeviceRequest();
  void MaybeStopSourceForRestart(
      const Vector<media::VideoCaptureFormat>& formats);
  void MaybeSourceStoppedForRestart(
      blink::MediaStreamVideoSource::RestartResult result);
  void FindNewFormatAndRestart(
      const Vector<media::VideoCaptureFormat>& formats);
  void MaybeSourceRestarted(
      blink::MediaStreamVideoSource::RestartResult result);

  // Helpers for all video requests.
  void ProcessVideoRequest();
  blink::MediaStreamVideoTrack* GetCurrentVideoTrack();
  blink::MediaStreamVideoSource* GetCurrentVideoSource();
  bool AbortIfVideoRequestStateInvalid();  // Returns true if aborted.
  blink::VideoCaptureSettings SelectVideoSettings(
      Vector<media::VideoCaptureFormat> formats);
  void FinalizeVideoRequest();

  // Helpers for audio requests.
  void ProcessAudioRequest();
  blink::MediaStreamAudioSource* GetCurrentAudioSource();

  // General helpers
  void ApplyConstraintsSucceeded();
  void ApplyConstraintsFailed(const char* failed_constraint_name);
  void CannotApplyConstraints(const String& message);
  void CleanupRequest(base::OnceClosure user_media_request_callback);
  blink::mojom::blink::MediaDevicesDispatcherHost* GetMediaDevicesDispatcher();

  // ApplyConstraints requests are processed sequentially. |current_request_|
  // contains the request currently being processed, if any.
  // |video_source_| and |request_completed_cb_| are the video source and
  // reply callback for the current request.
  Member<blink::ApplyConstraintsRequest> current_request_;

  // TODO(crbug.com/704136): Change to use Member.
  blink::MediaStreamVideoSource* video_source_ = nullptr;
  base::OnceClosure request_completed_cb_;

  MediaDevicesDispatcherCallback media_devices_dispatcher_cb_;
  THREAD_CHECKER(thread_checker_);

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ApplyConstraintsProcessor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_APPLY_CONSTRAINTS_PROCESSOR_H_
