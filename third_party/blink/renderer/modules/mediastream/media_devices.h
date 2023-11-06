// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_DEVICES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_DEVICES_H_

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver_with_tracker.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/mediastream/media_device_info.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/sub_capture_target.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class CaptureHandleConfig;
class DisplayMediaStreamOptions;
class ExceptionState;
class LocalFrame;
class Navigator;
class MediaTrackSupportedConstraints;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;
class UserMediaStreamConstraints;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EnumerateDevicesResult {
  kOk = 0,
  kUnknownError = 1,
  kErrorCaptureServiceCrash = 2,
  kErrorMediaDevicesDispatcherHostDisconnected = 3,
  kTimedOut = 4,
  kMaxValue = kTimedOut
};

class MODULES_EXPORT MediaDevices final
    : public EventTarget,
      public ActiveScriptWrappable<MediaDevices>,
      public Supplement<Navigator>,
      public ExecutionContextLifecycleObserver,
      public mojom::blink::MediaDevicesListener {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using SubCaptureTargetType = media::mojom::blink::SubCaptureTargetType;

  static const char kSupplementName[];
  static MediaDevices* mediaDevices(Navigator&);
  explicit MediaDevices(Navigator&);
  ~MediaDevices() override;

  ScriptPromise enumerateDevices(ScriptState*, ExceptionState&);
  MediaTrackSupportedConstraints* getSupportedConstraints() const;
  ScriptPromise getUserMedia(ScriptState*,
                             const UserMediaStreamConstraints*,
                             ExceptionState&);
  ScriptPromise SendUserMediaRequest(
      UserMediaRequestType,
      ScriptPromiseResolverWithTracker<UserMediaRequestResult>*,
      const MediaStreamConstraints*,
      ExceptionState&);

  ScriptPromise getAllScreensMedia(ScriptState*, ExceptionState&);

  ScriptPromise getDisplayMedia(ScriptState*,
                                const DisplayMediaStreamOptions*,
                                ExceptionState&);

  void setCaptureHandleConfig(ScriptState*,
                              const CaptureHandleConfig*,
                              ExceptionState&);

  // Using ProduceSubCaptureTarget(), CropTarget.fromElement() and similar
  // static functions can communicate with the browser process through
  // the mojom pipe that `this` owns.
  // TODO(crbug.com/1332628): Move most of the logic
  // into sub_capture_target.cc/h, leaving only communication in MediaDevices.
  ScriptPromise ProduceSubCaptureTarget(ScriptState*,
                                        Element*,
                                        ExceptionState&,
                                        SubCaptureTargetType);

  // EventTarget overrides.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;
  void RemoveAllEventListeners() override;

  // ScriptWrappable
  bool HasPendingActivity() const override;

  // ExecutionContextLifecycleObserver overrides.
  void ContextDestroyed() override;

  // mojom::blink::MediaDevicesListener implementation.
  void OnDevicesChanged(mojom::blink::MediaDeviceType,
                        const Vector<WebMediaDeviceInfo>&) override;

  void SetDispatcherHostForTesting(
      mojo::PendingRemote<mojom::blink::MediaDevicesDispatcherHost>);

  void Trace(Visitor*) const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(devicechange, kDevicechange)

 protected:
  // EventTarget overrides.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;
  void RemovedEventListener(const AtomicString& event_type,
                            const RegisteredEventListener&) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaDevicesTest, ObserveDeviceChangeEvent);

  void ScheduleDispatchEvent(Event*);
  void DispatchScheduledEvents();
  void StartObserving();
  void FinalizeStartObserving(
      const Vector<Vector<WebMediaDeviceInfo>>& enumeration,
      Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>
          video_input_capabilities,
      Vector<mojom::blink::AudioInputDeviceCapabilitiesPtr>
          audio_input_capabilities);
  void StopObserving();
  void DevicesEnumerated(
      ScriptPromiseResolverWithTracker<EnumerateDevicesResult>* result_tracker,
      const Vector<Vector<WebMediaDeviceInfo>>&,
      Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>,
      Vector<mojom::blink::AudioInputDeviceCapabilitiesPtr>);
  void OnDispatcherHostConnectionError();
  mojom::blink::MediaDevicesDispatcherHost& GetDispatcherHost(LocalFrame*);

#if !BUILDFLAG(IS_ANDROID)
  using ElementToResolverMap =
      HeapHashMap<Member<Element>, Member<ScriptPromiseResolver>>;

  // Each SubCaptureTarget sub-type has a map that associates Elements
  // with Promises for the asynchronous production of SubCaptureTargets.
  ElementToResolverMap& GetResolverMap(SubCaptureTargetType type);

  // Manage the window of opportunity that occurs immediately after
  // display-capture starts. The application can call
  // CaptureController.setFocusBehavior() on the microtask where the
  // Promise<MediaStream> was resolved; later calls raise an exception.
  // |id| identifies the source, and therefore the track, on the browser-side.
  void EnqueueMicrotaskToCloseFocusWindowOfOpportunity(const String&,
                                                       CaptureController*);
  void CloseFocusWindowOfOpportunity(const String&, CaptureController*);

  // Callback for receiving a message from the browser process with
  // the base::Token which is backing a SubCaptureTarget (either CropTarget
  // or RestrictionTarget).
  void ResolveSubCaptureTargetPromise(Element* element,
                                      SubCaptureTargetType type,
                                      const WTF::String& id);
#endif

  SEQUENCE_CHECKER(sequence_checker_);
  bool stopped_;
  // Async runner may be null when there is no valid execution context.
  // No async work may be posted in this scenario.
  TaskHandle dispatch_scheduled_events_task_handle_;
  HeapVector<Member<Event>> scheduled_events_;
  HeapMojoRemote<mojom::blink::MediaDevicesDispatcherHost> dispatcher_host_;
  HeapMojoReceiver<mojom::blink::MediaDevicesListener, MediaDevices> receiver_;
  HeapHashSet<Member<ScriptPromiseResolverWithTracker<EnumerateDevicesResult>>>
      enumerate_device_requests_;

#if !BUILDFLAG(IS_ANDROID)
  // 1. When CropTarget.fromElement() is first called for an Element,
  //    it has no CropTarget associated with it, and similarly for
  //    RestrictionTarget.fromElement(). For either of these, we produce
  //    a Resolver, map the Element to it, and fire off a message to
  //    the browser process, asking for a new base::Token to be generated.
  //    This base::Token, once produced, will serve as the underlying
  //    implementation of the CropTarget/RestrictionTarget object to which
  //    the Promise will be resolved.
  // 2. Subsequent calls to X.fromElement() which occur before the browser
  //    process has had time to respond, yield a copy of the original Promise
  //    associated with this Element. (Distinctly for either X=CropTarget
  //    and X=RestrictionTarget.)
  // 3. When the browser process responds with a base::Token, we store it
  //    on the Element itself, resolve all Promises returned for this Element,
  //    and eject the resolver from this container. (Note again that CropTarget
  //    and RestrictionTarget are handled separately here.)
  // 4. Later calls to X.fromElement() for this given Element discover that
  //    a token has already been assigned. They immediately return a resolved
  //    Promise with the relevant token.
  ElementToResolverMap crop_target_resolvers_;
  ElementToResolverMap restriction_target_resolvers_;
#endif

  bool starting_observation_ = false;
  Vector<Vector<WebMediaDeviceInfo>> current_device_infos_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_DEVICES_H_
