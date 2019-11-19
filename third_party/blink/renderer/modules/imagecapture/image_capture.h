// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGECAPTURE_IMAGE_CAPTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGECAPTURE_IMAGE_CAPTURE_H_

#include <memory>
#include "media/capture/mojom/image_capture.mojom-blink.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/imagecapture/photo_settings.h"
#include "third_party/blink/renderer/modules/mediastream/media_track_capabilities.h"
#include "third_party/blink/renderer/modules/mediastream/media_track_constraint_set.h"
#include "third_party/blink/renderer/modules/mediastream/media_track_settings.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class ExceptionState;
class ImageCaptureFrameGrabber;
class MediaStreamTrack;
class PhotoCapabilities;
class ScriptPromiseResolver;

// TODO(mcasas): Consider adding a web test checking that this class is not
// garbage collected while it has event listeners.
class MODULES_EXPORT ImageCapture final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<ImageCapture>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ImageCapture);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ImageCapture* Create(ExecutionContext*,
                              MediaStreamTrack*,
                              ExceptionState&);

  ImageCapture(ExecutionContext*, MediaStreamTrack*);
  ~ImageCapture() override;

  // EventTarget implementation.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ScriptWrappable implementation.
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  MediaStreamTrack* videoStreamTrack() const { return stream_track_.Get(); }

  ScriptPromise getPhotoCapabilities(ScriptState*);
  ScriptPromise getPhotoSettings(ScriptState*);

  ScriptPromise setOptions(ScriptState*,
                           const PhotoSettings*,
                           bool trigger_take_photo = false);

  ScriptPromise takePhoto(ScriptState*);
  ScriptPromise takePhoto(ScriptState*, const PhotoSettings*);

  ScriptPromise grabFrame(ScriptState*);

  MediaTrackCapabilities* GetMediaTrackCapabilities() const;
  void SetMediaTrackConstraints(
      ScriptPromiseResolver*,
      const HeapVector<Member<MediaTrackConstraintSet>>&);
  const MediaTrackConstraintSet* GetMediaTrackConstraints() const;
  void ClearMediaTrackConstraints();
  void GetMediaTrackSettings(MediaTrackSettings*) const;

  void Trace(blink::Visitor*) override;

 private:
  using PromiseResolverFunction =
      base::OnceCallback<void(ScriptPromiseResolver*)>;

  void OnMojoGetPhotoState(ScriptPromiseResolver*,
                           PromiseResolverFunction,
                           bool trigger_take_photo,
                           media::mojom::blink::PhotoStatePtr);
  void OnMojoSetOptions(ScriptPromiseResolver*,
                        bool trigger_take_photo,
                        bool result);
  void OnMojoTakePhoto(ScriptPromiseResolver*, media::mojom::blink::BlobPtr);

  void UpdateMediaTrackCapabilities(media::mojom::blink::PhotoStatePtr);
  void OnServiceConnectionError();

  void ResolveWithNothing(ScriptPromiseResolver*);
  void ResolveWithPhotoSettings(ScriptPromiseResolver*);
  void ResolveWithPhotoCapabilities(ScriptPromiseResolver*);

  Member<MediaStreamTrack> stream_track_;
  std::unique_ptr<ImageCaptureFrameGrabber> frame_grabber_;
  mojo::Remote<media::mojom::blink::ImageCapture> service_;

  Member<MediaTrackCapabilities> capabilities_;
  Member<MediaTrackSettings> settings_;
  Member<MediaTrackConstraintSet> current_constraints_;
  Member<PhotoSettings> photo_settings_;

  Member<PhotoCapabilities> photo_capabilities_;

  HeapHashSet<Member<ScriptPromiseResolver>> service_requests_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGECAPTURE_IMAGE_CAPTURE_H_
