// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_DEVICES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_DEVICES_H_

#include "base/callback.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/mediastream/media_device_info.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExceptionState;
class LocalFrame;
class Navigator;
class MediaStreamConstraints;
class MediaTrackSupportedConstraints;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT MediaDevices final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<MediaDevices>,
      public Supplement<Navigator>,
      public ExecutionContextLifecycleObserver,
      public mojom::blink::MediaDevicesListener {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  static MediaDevices* mediaDevices(Navigator&);
  explicit MediaDevices(Navigator&);
  ~MediaDevices() override;

  ScriptPromise enumerateDevices(ScriptState*, ExceptionState&);
  MediaTrackSupportedConstraints* getSupportedConstraints() const;
  ScriptPromise getUserMedia(ScriptState*,
                             const MediaStreamConstraints*,
                             ExceptionState&);
  ScriptPromise SendUserMediaRequest(ScriptState*,
                                     UserMediaRequest::MediaType,
                                     const MediaStreamConstraints*,
                                     ExceptionState&);

  ScriptPromise getDisplayMedia(ScriptState*,
                                const MediaStreamConstraints*,
                                ExceptionState&);

  ScriptPromise getCurrentBrowsingContextMedia(ScriptState*,
                                               const MediaStreamConstraints*,
                                               ExceptionState&);

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

  // Callback for testing only.
  using EnumerateDevicesTestCallback =
      base::OnceCallback<void(const MediaDeviceInfoVector&)>;

  void SetDispatcherHostForTesting(
      mojo::PendingRemote<mojom::blink::MediaDevicesDispatcherHost>);

  void SetEnumerateDevicesCallbackForTesting(
      EnumerateDevicesTestCallback test_callback) {
    enumerate_devices_test_callback_ = std::move(test_callback);
  }

  void SetConnectionErrorCallbackForTesting(base::OnceClosure test_callback) {
    connection_error_test_callback_ = std::move(test_callback);
  }

  void SetDeviceChangeCallbackForTesting(base::OnceClosure test_callback) {
    device_change_test_callback_ = std::move(test_callback);
  }

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
  void StopObserving();
  void DevicesEnumerated(ScriptPromiseResolver*,
                         const Vector<Vector<WebMediaDeviceInfo>>&,
                         Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>,
                         Vector<mojom::blink::AudioInputDeviceCapabilitiesPtr>);
  void OnDispatcherHostConnectionError();
  const mojo::Remote<mojom::blink::MediaDevicesDispatcherHost>&
  GetDispatcherHost(LocalFrame*);

  bool stopped_;
  // Async runner may be null when there is no valid execution context.
  // No async work may be posted in this scenario.
  TaskHandle dispatch_scheduled_events_task_handle_;
  HeapVector<Member<Event>> scheduled_events_;
  mojo::Remote<mojom::blink::MediaDevicesDispatcherHost> dispatcher_host_;
  HeapMojoReceiver<mojom::blink::MediaDevicesListener, MediaDevices> receiver_;
  HeapHashSet<Member<ScriptPromiseResolver>> requests_;

  EnumerateDevicesTestCallback enumerate_devices_test_callback_;
  base::OnceClosure connection_error_test_callback_;
  base::OnceClosure device_change_test_callback_;
};

}  // namespace blink

#endif
