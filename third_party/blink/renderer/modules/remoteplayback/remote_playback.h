// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_REMOTEPLAYBACK_REMOTE_PLAYBACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_REMOTEPLAYBACK_REMOTE_PLAYBACK_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom-blink.h"
#include "third_party/blink/public/platform/modules/remoteplayback/web_remote_playback_client.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/media/remote_playback_controller.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability_observer.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class AvailabilityCallbackWrapper;
class HTMLMediaElement;
class ScriptPromiseResolver;
class ScriptState;
class V8RemotePlaybackAvailabilityCallback;

// Remote playback for HTMLMediaElements.
// The new RemotePlayback pipeline is implemented on top of Presentation.
// - This class uses PresentationAvailability to detect potential devices to
//   initiate remote playback for a media element.
// - A remote playback session is implemented as a PresentationConnection.
class MODULES_EXPORT RemotePlayback final
    : public EventTargetWithInlineData,
      public ContextLifecycleObserver,
      public ActiveScriptWrappable<RemotePlayback>,
      public WebRemotePlaybackClient,
      public PresentationAvailabilityObserver,
      public mojom::blink::PresentationConnection,
      public RemotePlaybackController {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(RemotePlayback);

 public:
  // Result of WatchAvailabilityInternal that means availability is not
  // supported.
  static const int kWatchAvailabilityNotSupported = -1;

  static RemotePlayback& From(HTMLMediaElement&);

  explicit RemotePlayback(HTMLMediaElement&);

  // Notifies this object that disableRemotePlayback attribute was set on the
  // corresponding media element.
  void RemotePlaybackDisabled();

  // EventTarget implementation.
  const WTF::AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // Starts notifying the page about the changes to the remote playback devices
  // availability via the provided callback. May start the monitoring of remote
  // playback devices if it isn't running yet.
  ScriptPromise watchAvailability(ScriptState*,
                                  V8RemotePlaybackAvailabilityCallback*);

  // Cancels updating the page via the callback specified by its id.
  ScriptPromise cancelWatchAvailability(ScriptState*, int id);

  // Cancels all the callbacks watching remote playback availability changes
  // registered with this element.
  ScriptPromise cancelWatchAvailability(ScriptState*);

  // Shows the UI allowing user to change the remote playback state of the media
  // element (by picking a remote playback device from the list, for example).
  ScriptPromise prompt(ScriptState*);

  String state() const;

  // The implementation of prompt(). Used by the native remote playback button.
  void PromptInternal();

  // The implementation of watchAvailability() and cancelWatchAvailability().
  // Can return kWatchAvailabilityNotSupported to indicate the availability
  // monitoring is disabled. RemotePlaybackAvailable() will return true then.
  int WatchAvailabilityInternal(AvailabilityCallbackWrapper*);
  bool CancelWatchAvailabilityInternal(int id);

  mojom::blink::PresentationConnectionState GetState() const { return state_; }

  // PresentationAvailabilityObserver implementation.
  void AvailabilityChanged(mojom::blink::ScreenAvailability) override;
  const Vector<KURL>& Urls() const override;

  // Handles the response from PresentationService::StartPresentation.
  void HandlePresentationResponse(mojom::blink::PresentationConnectionResultPtr,
                                  mojom::blink::PresentationErrorPtr);
  void OnConnectionSuccess(mojom::blink::PresentationConnectionResultPtr);
  void OnConnectionError(const mojom::blink::PresentationError&);

  // mojom::blink::PresentationConnection implementation.
  void OnMessage(mojom::blink::PresentationConnectionMessagePtr) override;
  void DidChangeState(mojom::blink::PresentationConnectionState) override;
  void DidClose(mojom::blink::PresentationConnectionCloseReason) override;

  // WebRemotePlaybackClient implementation.
  bool RemotePlaybackAvailable() const override;
  void SourceChanged(const WebURL&, bool is_source_supported) override;
  WebString GetPresentationId() override;

  // RemotePlaybackController implementation.
  void AddObserver(RemotePlaybackObserver*) override;
  void RemoveObserver(RemotePlaybackObserver*) override;
  void AvailabilityChangedForTesting(bool screen_is_available) override;
  void StateChangedForTesting(bool is_connected) override;

  // ScriptWrappable implementation.
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver implementation.
  void ContextDestroyed(ExecutionContext*) override;

  // Adjusts the internal state of |this| after a playback state change.
  void StateChanged(mojom::blink::PresentationConnectionState);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(connecting, kConnecting)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(connect, kConnect)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect, kDisconnect)

  void Trace(blink::Visitor*) override;

 private:
  friend class V8RemotePlayback;
  friend class RemotePlaybackTest;
  friend class MediaControlsImplTest;

  void PromptCancelled();

  // Calls the specified availability callback with the current availability.
  // Need a void() method to post it as a task.
  void NotifyInitialAvailability(int callback_id);

  // Starts listening for remote playback device availability if there're both
  // registered availability callbacks and a valid source set. May be called
  // more than once in a row.
  void MaybeStartListeningForAvailability();

  // Stops listening for remote playback device availability (unconditionally).
  // May be called more than once in a row.
  void StopListeningForAvailability();

  // Clears bindings after remote playback stops.
  void CleanupConnections();

  mojom::blink::PresentationConnectionState state_;
  mojom::blink::ScreenAvailability availability_;
  HeapHashMap<int, Member<AvailabilityCallbackWrapper>> availability_callbacks_;
  Member<HTMLMediaElement> media_element_;
  Member<ScriptPromiseResolver> prompt_promise_resolver_;
  Vector<KURL> availability_urls_;
  bool is_listening_;

  String presentation_id_;
  KURL presentation_url_;

  mojo::Receiver<mojom::blink::PresentationConnection>
      presentation_connection_receiver_{this};
  mojo::Remote<mojom::blink::PresentationConnection>
      target_presentation_connection_;

  HeapHashSet<Member<RemotePlaybackObserver>> observers_;

  DISALLOW_COPY_AND_ASSIGN(RemotePlayback);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_REMOTEPLAYBACK_REMOTE_PLAYBACK_H_
