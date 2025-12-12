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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_GEOLOCATION_GEOLOCATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_GEOLOCATION_GEOLOCATION_H_

#include "base/dcheck_is_on.h"
#include "base/types/expected.h"
#include "services/device/public/mojom/geolocation.mojom-blink.h"
#include "third_party/blink/public/mojom/geolocation/geolocation_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_position_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_position_error_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_position_options.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/geolocation/geo_notifier.h"
#include "third_party/blink/renderer/core/geolocation/geolocation_position_error.h"
#include "third_party/blink/renderer/core/geolocation/geolocation_watchers.h"
#include "third_party/blink/renderer/core/geolocation/geoposition.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

namespace mojom {
enum class PermissionStatus;
}  // namespace mojom

class LocalFrame;
class Navigator;

class CORE_EXPORT Geolocation final
    : public ScriptWrappable,
      public ActiveScriptWrappable<Geolocation>,
      public Supplement<Navigator>,
      public ExecutionContextLifecycleObserver,
      public PageVisibilityObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  static Geolocation* geolocation(Navigator&);

  explicit Geolocation(Navigator&);
  ~Geolocation() override;
  void Trace(Visitor*) const override;

  // Inherited from ExecutionContextLifecycleObserver and
  // PageVisibilityObserver.
  void ContextDestroyed() override;

  LocalFrame* GetFrame() const;

  // Creates a oneshot and attempts to obtain a position that meets the
  // constraints of the options. This method gets called when the geolocation
  // API is invoked from script.
  void getCurrentPositionForBindings(
      V8PositionCallback*,
      V8PositionErrorCallback* = nullptr,
      const PositionOptions* = PositionOptions::Create());

  // Creates a oneshot and attempts to obtain a position that meets the
  // constraints of the options. This method gets called when the geolocation
  // API is invoked from Blink.
  void GetCurrentPosition(
      base::RepeatingCallback<
          void(base::expected<Geoposition*, GeolocationPositionError*>)>,
      const PositionOptions* = PositionOptions::Create());

  // Creates a watcher that will be notified whenever a new position is
  // available that meets the constraints of the options. This method gets
  // called when the geolocation API is invoked from script.
  int watchPositionForBindings(
      V8PositionCallback*,
      V8PositionErrorCallback* = nullptr,
      const PositionOptions* = PositionOptions::Create());

  // Creates a watcher that will be notified whenever a new position is
  // available that meets the constraints of the options. This method gets
  // called when the geolocation API is invoked from Blink.
  // Returns an watch ID, which can be used to clear the watcher.
  int WatchPosition(
      base::RepeatingCallback<
          void(base::expected<Geoposition*, GeolocationPositionError*>)>,
      const PositionOptions* = PositionOptions::Create());

  int WatchPositionInternal(GeoNotifier* notifier);

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
  class GeoNotifierSet final : public GarbageCollected<GeoNotifierSet> {
   public:
    void Trace(Visitor* visitor) const { visitor->Trace(set_); }

    auto begin() const { return set_.begin(); }
    auto end() const { return set_.end(); }
    auto size() const { return set_.size(); }

    auto insert(GeoNotifier* value) {
      DCHECK(!value->IsTimerActive());
      return set_.insert(value);
    }

    void erase(GeoNotifier* value) {
      DCHECK(!value->IsTimerActive());
      return set_.erase(value);
    }

    void clear() {
#if DCHECK_IS_ON()
      for (const auto& notifier : set_) {
        DCHECK(!notifier->IsTimerActive());
      }
#endif
      set_.clear();
    }

    auto Contains(GeoNotifier* value) const { return set_.Contains(value); }
    auto IsEmpty() const { return set_.empty(); }

    auto InsertWithoutTimerCheck(GeoNotifier* value) {
      return set_.insert(value);
    }
    void ClearWithoutTimerCheck() { set_.clear(); }

   private:
    HeapHashSet<Member<GeoNotifier>> set_;
  };

  bool HasListeners() const {
    return !one_shots_->IsEmpty() || !watchers_->IsEmpty();
  }

  void StopTimers();

  // Runs the success callbacks on all notifiers. A position must be available
  // and the user must have given permission.
  void MakeSuccessCallbacks();

  // and also  clears the watchers if the error is fatal.
  void HandleError(GeolocationPositionError*);

  // Ensures location updates are active and accuracy is
  // always configured based on the current set of requests. This central
  // function should be called whenever active notifiers change or permission is
  // granted.
  // If the connection to the Geolocation service is not yet established, it
  // initiates the connection and permission request, then returns early. This
  // function is called again once the permission status is resolved.
  void UpdateGeolocationState();

  // Tears down the connection to the Geolocation service and stops all timers.
  void StopUpdating();

  // Establishes a connection to the Geolocation service and request permission.
  // This is an asynchronous operation. Returns true if the connection is
  // already established. Returns false if the connection is not yet
  // established, In this case, permission will be reuested and
  // OnGeolocationPermissionStatusUpdated will be called upon completion.
  bool EnsureGeolocationConnection();

  // Resets the connection to the Geolocation service.
  void ResetGeolocationConnection();
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

  void OnPositionUpdated(device::mojom::blink::GeopositionResultPtr);

  void OnGeolocationConnectionError();

  // Callback for the asynchronous permission request. Upon invoked, it proceeds
  // with location updates or handles the error.
  void OnGeolocationPermissionStatusUpdated(mojom::blink::PermissionStatus);

  void HandlePermissionError();

  void UpdateAccuracyHint();

  Member<GeoNotifierSet> one_shots_;
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
  Member<GeoNotifierSet> one_shots_being_invoked_;
  HeapVector<Member<GeoNotifier>> watchers_being_invoked_;
  Member<Geoposition> last_position_;

  HeapMojoRemote<device::mojom::blink::Geolocation> geolocation_;
  HeapMojoRemote<mojom::blink::GeolocationService> geolocation_service_;
  bool enable_high_accuracy_ = false;

  // Whether a QueryNextPosition request sent and we are waiting for a position
  // update.
  bool updating_ = false;
  bool permission_request_in_progress_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_GEOLOCATION_GEOLOCATION_H_
