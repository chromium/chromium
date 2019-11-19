// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/page/focus_changed_observer.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_session_init.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ScriptPromiseResolver;
class XRFrameProvider;
class XRSessionInit;

class XR final : public EventTargetWithInlineData,
                 public ContextLifecycleObserver,
                 public device::mojom::blink::VRServiceClient,
                 public FocusChangedObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(XR);

 public:
  // TODO(crbug.com/976796): Fix lint errors.
  static XR* Create(LocalFrame& frame, int64_t source_id) {
    return MakeGarbageCollected<XR>(frame, source_id);
  }

  // TODO(crbug.com/976796): Fix lint errors.
  XR(LocalFrame& frame, int64_t ukm_source_id);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(devicechange, kDevicechange)

  ScriptPromise supportsSession(ScriptState*,
                                const String&,
                                ExceptionState& exception_state);
  ScriptPromise isSessionSupported(ScriptState*,
                                   const String&,
                                   ExceptionState& exception_state);
  ScriptPromise requestSession(ScriptState*,
                               const String&,
                               XRSessionInit*,
                               ExceptionState& exception_state);

  XRFrameProvider* frameProvider();

  const mojo::AssociatedRemote<
      device::mojom::blink::XREnvironmentIntegrationProvider>&
  xrEnvironmentProviderRemote();

  // VRServiceClient overrides.
  void OnDeviceChanged() override;

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;
  void Trace(blink::Visitor*) override;

  // FocusChangedObserver overrides.
  void FocusedFrameChanged() override;
  bool IsFrameFocused();

  int64_t GetSourceId() const { return ukm_source_id_; }

  using EnvironmentProviderErrorCallback = base::OnceCallback<void()>;
  // Registers a callback that'll be invoked when mojo invokes a disconnect
  // handler on the underlying XREnvironmentIntegrationProvider remote.
  void AddEnvironmentProviderErrorHandler(
      EnvironmentProviderErrorCallback callback);

  void ExitPresent(base::OnceClosure on_exited);

  void SetFramesThrottled(const XRSession* session, bool throttled);

  base::TimeTicks NavigationStart() const { return navigation_start_; }

  bool IsContextDestroyed() const { return is_context_destroyed_; }

 private:
  enum SensorRequirement {
    kNone,
    kOptional,
    kRequired,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SessionRequestStatus : int {
    // `requestSession` succeeded.
    kSuccess = 0,
    // `requestSession` failed with other (unknown) error.
    kOtherError = 1,
    kMaxValue = kOtherError,
  };

  struct RequestedXRSessionFeatureSet {
    // Set of requested features which are known and valid for the current mode.
    XRSessionFeatureSet valid_features;

    // Whether any requested features were unknown or invalid
    bool invalid_features = false;
  };

  // Encapsulates blink-side `XR::requestSession()` call. It is a wrapper around
  // ScriptPromiseResolver that allows us to add additional logic as certain
  // things related to promise's life cycle happen.
  class PendingRequestSessionQuery final
      : public GarbageCollected<PendingRequestSessionQuery> {
   public:
    PendingRequestSessionQuery(int64_t ukm_source_id,
                               ScriptPromiseResolver* resolver,
                               XRSession::SessionMode mode,
                               RequestedXRSessionFeatureSet required_features,
                               RequestedXRSessionFeatureSet optional_features);
    virtual ~PendingRequestSessionQuery() = default;

    // Resolves underlying promise with passed in XR session.
    // If metrics are to be recorded for this session, an
    // |XRSessionMetricsRecorded| may be passed in as well.
    void Resolve(
        XRSession* session,
        mojo::PendingRemote<device::mojom::blink::XRSessionMetricsRecorder>
            metrics_recorder = mojo::NullRemote());

    // Rejects underlying promise with a DOMException.
    // Do not call this with |DOMExceptionCode::kSecurityError|, use
    // |RejectWithSecurityError| for that. If the exception is thrown
    // synchronously, an ExceptionState must be passed in. Otherwise it may be
    // null.
    void RejectWithDOMException(DOMExceptionCode exception_code,
                                const String& message,
                                ExceptionState* exception_state);

    // Rejects underlying promise with a SecurityError.
    // If the exception is thrown synchronously, an ExceptionState must
    // be passed in. Otherwise it may be null.
    void RejectWithSecurityError(const String& sanitized_message,
                                 ExceptionState* exception_state);

    // Rejects underlying promise with a TypeError.
    // If the exception is thrown synchronously, an ExceptionState must
    // be passed in. Otherwise it may be null.
    void RejectWithTypeError(const String& message,
                             ExceptionState* exception_state);

    XRSession::SessionMode mode() const;
    const XRSessionFeatureSet& RequiredFeatures() const;
    const XRSessionFeatureSet& OptionalFeatures() const;
    bool InvalidRequiredFeatures() const;
    bool InvalidOptionalFeatures() const;

    SensorRequirement GetSensorRequirement() const {
      return sensor_requirement_;
    }

    // Returns underlying resolver's script state.
    ScriptState* GetScriptState() const;

    virtual void Trace(blink::Visitor*);

   private:
    void ParseSensorRequirement();
    device::mojom::XRSessionFeatureRequestStatus GetFeatureRequestStatus(
        device::mojom::XRSessionFeature feature,
        const XRSession* session) const;
    void ReportRequestSessionResult(
        SessionRequestStatus status,
        XRSession* session = nullptr,
        mojo::PendingRemote<device::mojom::blink::XRSessionMetricsRecorder>
            metrics_recorder = mojo::NullRemote());

    Member<ScriptPromiseResolver> resolver_;
    const XRSession::SessionMode mode_;
    RequestedXRSessionFeatureSet required_features_;
    RequestedXRSessionFeatureSet optional_features_;
    SensorRequirement sensor_requirement_ = SensorRequirement::kNone;

    const int64_t ukm_source_id_;

    DISALLOW_COPY_AND_ASSIGN(PendingRequestSessionQuery);
  };

  static device::mojom::blink::XRSessionOptionsPtr XRSessionOptionsFromQuery(
      const PendingRequestSessionQuery& query);

  // Encapsulates blink-side `XR::supportsSession()` call.  It is a wrapper
  // around ScriptPromiseResolver that allows us to add additional logic as
  // certain things related to promise's life cycle happen.
  class PendingSupportsSessionQuery final
      : public GarbageCollected<PendingSupportsSessionQuery> {
   public:
    PendingSupportsSessionQuery(ScriptPromiseResolver*,
                                XRSession::SessionMode,
                                bool throw_on_unsupported);
    virtual ~PendingSupportsSessionQuery() = default;

    // Resolves underlying promise.
    void Resolve(bool supported, ExceptionState* exception_state = nullptr);

    // Rejects underlying promise with a DOMException.
    // Do not call this with |DOMExceptionCode::kSecurityError|, use
    // |RejectWithSecurityError| for that. If the exception is thrown
    // synchronously, an ExceptionState must be passed in. Otherwise it may be
    // null.
    void RejectWithDOMException(DOMExceptionCode exception_code,
                                const String& message,
                                ExceptionState* exception_state);

    // Rejects underlying promise with a SecurityError.
    // If the exception is thrown synchronously, an ExceptionState must
    // be passed in. Otherwise it may be null.
    void RejectWithSecurityError(const String& sanitized_message,
                                 ExceptionState* exception_state);

    // Rejects underlying promise with a TypeError.
    // If the exception is thrown synchronously, an ExceptionState must
    // be passed in. Otherwise it may be null.
    void RejectWithTypeError(const String& message,
                             ExceptionState* exception_state);

    bool ThrowOnUnsupported() const { return throw_on_unsupported_; }

    XRSession::SessionMode mode() const;

    virtual void Trace(blink::Visitor*);

   private:
    Member<ScriptPromiseResolver> resolver_;
    const XRSession::SessionMode mode_;

    // Only set when calling the deprecated supportsSession method.
    const bool throw_on_unsupported_ = false;

    DISALLOW_COPY_AND_ASSIGN(PendingSupportsSessionQuery);
  };

  ScriptPromise InternalIsSessionSupported(ScriptState*,
                                           const String&,
                                           ExceptionState& exception_state,
                                           bool throw_on_unsupported);

  const char* CheckInlineSessionRequestAllowed(
      LocalFrame* frame,
      const PendingRequestSessionQuery& query);

  RequestedXRSessionFeatureSet ParseRequestedFeatures(
      Document* doc,
      const HeapVector<ScriptValue>& features,
      const XRSession::SessionMode& session_mode,
      mojom::ConsoleMessageLevel error_level);

  void RequestImmersiveSession(LocalFrame* frame,
                               Document* doc,
                               PendingRequestSessionQuery* query,
                               ExceptionState* exception_state);

  void RequestInlineSession(LocalFrame* frame,
                            PendingRequestSessionQuery* query,
                            ExceptionState* exception_state);

  void OnRequestSessionReturned(
      PendingRequestSessionQuery*,
      device::mojom::blink::RequestSessionResultPtr result);
  void OnSupportsSessionReturned(PendingSupportsSessionQuery*,
                                 bool supports_session);

  void EnsureDevice();
  void ReportImmersiveSupported(bool supported);

  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

  XRSession* CreateSession(
      XRSession::SessionMode mode,
      XRSession::EnvironmentBlendMode blend_mode,
      mojo::PendingReceiver<device::mojom::blink::XRSessionClient>
          client_receiver,
      device::mojom::blink::VRDisplayInfoPtr display_info,
      bool uses_input_eventing,
      XRSessionFeatureSet enabled_features,
      bool sensorless_session = false);

  XRSession* CreateSensorlessInlineSession();

  enum class DisposeType {
    kContextDestroyed = 0,
    kDisconnected = 1,
  };
  void Dispose(DisposeType);

  void OnEnvironmentProviderDisconnect();

  // Indicates whether use of requestDevice has already been logged.
  bool did_log_supports_immersive_ = false;

  // Indicates whether we've already logged a request for an immersive session.
  bool did_log_request_immersive_session_ = false;

  const int64_t ukm_source_id_;

  HeapHashSet<Member<PendingSupportsSessionQuery>> outstanding_support_queries_;
  HeapHashSet<Member<PendingRequestSessionQuery>> outstanding_request_queries_;
  bool has_outstanding_immersive_request_ = false;

  Vector<EnvironmentProviderErrorCallback>
      environment_provider_error_callbacks_;

  Member<XRFrameProvider> frame_provider_;
  HeapHashSet<WeakMember<XRSession>> sessions_;
  mojo::Remote<device::mojom::blink::VRService> service_;
  mojo::AssociatedRemote<device::mojom::blink::XREnvironmentIntegrationProvider>
      environment_provider_;
  mojo::Receiver<device::mojom::blink::VRServiceClient> receiver_{this};

  // Time at which navigation started. Used as the base for relative timestamps,
  // such as for Gamepad objects.
  base::TimeTicks navigation_start_;

  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;

  // In DOM overlay mode, save and restore the FrameView background color.
  Color original_base_background_color_;

  bool is_context_destroyed_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_H_
