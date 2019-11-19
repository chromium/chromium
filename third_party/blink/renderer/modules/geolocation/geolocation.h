/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All Rights Reserved.
 * Copyright 2010, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_GEOLOCATION_GEOLOCATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_GEOLOCATION_GEOLOCATION_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/geolocation.mojom-blink.h"
#include "third_party/blink/public/mojom/geolocation/geolocation_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_position_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_position_error_callback.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/modules/geolocation/geo_notifier.h"
#include "third_party/blink/renderer/modules/geolocation/geolocation_position_error.h"
#include "third_party/blink/renderer/modules/geolocation/geolocation_watchers.h"
#include "third_party/blink/renderer/modules/geolocation/geoposition.h"
#include "third_party/blink/renderer/modules/geolocation/position_options.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

namespace mojom {
enum class PermissionStatus;
}  // namespace mojom

class Document;
class LocalFrame;
class ExecutionContext;

class MODULES_EXPORT Geolocation final
    : public ScriptWrappable,
      public ActiveScriptWrappable<Geolocation>,
      public ContextLifecycleObserver,
      public PageVisibilityObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Geolocation);

 public:
  static Geolocation* Create(ExecutionContext*);

  explicit Geolocation(ExecutionContext*);
  ~Geolocation() override;
  void Trace(blink::Visitor*) override;

  // Inherited from ContextLifecycleObserver and PageVisibilityObserver.
  void ContextDestroyed(ExecutionContext*) override;

  Document* GetDocument() const;
  LocalFrame* GetFrame() const;

  // Creates a oneshot and attempts to obtain a position that meets the
  // constraints of the options.
  void getCurrentPosition(V8PositionCallback*,
                          V8PositionErrorCallback* = nullptr,
                          const PositionOptions* = PositionOptions::Create());

  // Creates a watcher that will be notified whenever a new position is
  // available that meets the constraints of the options.
  int watchPosition(V8PositionCallback*,
                    V8PositionErrorCallback* = nullptr,
                    const PositionOptions* = PositionOptions::Create());

  // Removes all references to the watcher, it will not be updated again.
  void clearWatch(int watch_id);

  // Notifies this that a new position is available. Must never be called
  // before permission is granted by the user.
  void PositionChanged();

  // Discards the notifier because a fatal error occurred for it.
  void FatalErrorOccurred(GeoNotifier*);

  // Adds the notifier to the set awaiting a cached position. Runs the success
  // callbacks for them if permission has been granted. Requests permission if
  // it is unknown.
  void RequestUsesCachedPosition(GeoNotifier*);

  // Discards the notifier if it is a oneshot because it timed it.
  void RequestTimedOut(GeoNotifier*);

  // Returns true if this geolocation still owns the given notifier.
  bool DoesOwnNotifier(GeoNotifier*) const;

  // Inherited from PageVisibilityObserver.
  void PageVisibilityChanged() override;

  // TODO(yukishiino): This is a short-term speculative fix for
  // crbug.com/792604. Remove this once the bug is fixed.
  bool HasPendingActivity() const final;

 private:
  // Customized HeapHashSet class that checks notifiers' timers. Notifier's
  // timer may be active only when the notifier is owned by the Geolocation.
  class GeoNotifierSet : private HeapHashSet<Member<GeoNotifier>> {
    using BaseClass = HeapHashSet<Member<GeoNotifier>>;

   public:
    using BaseClass::Trace;

    using BaseClass::const_iterator;
    using BaseClass::iterator;

    using BaseClass::begin;
    using BaseClass::end;
    using BaseClass::size;

    auto insert(GeoNotifier* value) {
      DCHECK(!value->IsTimerActive());
      return BaseClass::insert(value);
    }

    void erase(GeoNotifier* value) {
      DCHECK(!value->IsTimerActive());
      return BaseClass::erase(value);
    }

    void clear() {
#if DCHECK_IS_ON()
      for (const auto& notifier : *this) {
        DCHECK(!notifier->IsTimerActive());
      }
#endif
      BaseClass::clear();
    }

    using BaseClass::Contains;
    using BaseClass::IsEmpty;

    auto InsertWithoutTimerCheck(GeoNotifier* value) {
      return BaseClass::insert(value);
    }
    void ClearWithoutTimerCheck() { BaseClass::clear(); }
  };

  bool HasListeners() const {
    return !one_shots_.IsEmpty() || !watchers_->IsEmpty();
  }

  void StopTimers();

  // Runs the success callbacks on all notifiers. A position must be available
  // and the user must have given permission.
  void MakeSuccessCallbacks();

  // Sends the given error to all notifiers, unless the error is not fatal and
  // the notifier is due to receive a cached position. Clears the oneshots,
  // and also  clears the watchers if the error is fatal.
  void HandleError(GeolocationPositionError*);

  // Connects to the Geolocation mojo service and starts polling for updates.
  void StartUpdating(GeoNotifier*);

  void StopUpdating();

  void UpdateGeolocationConnection(GeoNotifier*);
  void QueryNextPosition();

  // Attempts to obtain a position for the given notifier, either by using
  // the cached position or by requesting one from the Geolocation service.
  // Sets a fatal error if permission is denied or no position can be
  // obtained.
  void StartRequest(GeoNotifier*);

  bool HaveSuitableCachedPosition(const PositionOptions*);

  // Record whether the origin trying to access Geolocation would be
  // allowed to access a feature that can only be accessed by secure origins.
  // See https://goo.gl/Y0ZkNV
  void RecordOriginTypeAccess() const;

  void OnPositionUpdated(device::mojom::blink::GeopositionPtr);

  void OnGeolocationConnectionError();

  void OnGeolocationPermissionStatusUpdated(GeoNotifier*,
                                            mojom::PermissionStatus);

  GeoNotifierSet one_shots_;
  Member<GeolocationWatchers> watchers_;
  // GeoNotifiers that are in the middle of invocation.
  //
  // |HandleError(error)| and |MakeSuccessCallbacks| need to clear |one_shots_|
  // (and optionally |watchers_|) before invoking the callbacks, in order to
  // avoid clearing notifiers added by calls to Geolocation methods
  // from the callbacks. Thus, something else needs to make the notifiers being
  // invoked alive with wrapper-tracing because V8 GC may run during the
  // callbacks. |one_shots_being_invoked_| and |watchers_being_invoked_| perform
  // wrapper-tracing.
  // TODO(https://crbug.com/796145): Remove this hack once on-stack objects
  // get supported by either of wrapper-tracing or unified GC.
  GeoNotifierSet one_shots_being_invoked_;
  HeapVector<Member<GeoNotifier>> watchers_being_invoked_;
  Member<Geoposition> last_position_;

  mojo::Remote<device::mojom::blink::Geolocation> geolocation_;
  mojo::Remote<mojom::blink::GeolocationService> geolocation_service_;
  bool enable_high_accuracy_ = false;

  // Whether a GeoNotifier is waiting for a position update.
  bool updating_ = false;

  // Set to true when |geolocation_| is disconnected. This is used to
  // detect when |geolocation_| is disconnected and reconnected while
  // running callbacks in response to a call to OnPositionUpdated().
  bool disconnected_geolocation_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_GEOLOCATION_GEOLOCATION_H_
