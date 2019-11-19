// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_GEOLOCATION_GEO_NOTIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_GEOLOCATION_GEO_NOTIFIER_H_

#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_position_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_position_error_callback.h"
#include "third_party/blink/renderer/modules/geolocation/position_options.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class Geolocation;
class GeolocationPositionError;
class Geoposition;

class GeoNotifier final : public GarbageCollected<GeoNotifier>,
                          public NameClient {
 public:
  GeoNotifier(Geolocation*,
              V8PositionCallback*,
              V8PositionErrorCallback*,
              const PositionOptions*);
  ~GeoNotifier() = default;
  void Trace(blink::Visitor*);
  const char* NameInHeapSnapshot() const override { return "GeoNotifier"; }

  const PositionOptions* Options() const { return options_; }

  // Sets the given error as the fatal error if there isn't one yet.
  // Starts the timer with an interval of 0.
  void SetFatalError(GeolocationPositionError*);

  bool UseCachedPosition() const { return use_cached_position_; }

  // Tells the notifier to use a cached position and starts its timer with
  // an interval of 0.
  void SetUseCachedPosition();

  void RunSuccessCallback(Geoposition*);
  void RunErrorCallback(GeolocationPositionError*);

  void StartTimer();
  void StopTimer();
  bool IsTimerActive() const;

 private:
  // Customized TaskRunnerTimer class that checks the ownership between this
  // notifier and the Geolocation. The timer should run only when the notifier
  // is owned by the Geolocation. When the Geolocation removes a notifier, the
  // timer should be stopped beforehand.
  class Timer final : public GarbageCollected<Timer> {
   public:
    explicit Timer(scoped_refptr<base::SingleThreadTaskRunner> web_task_runner,
                   GeoNotifier* notifier,
                   void (GeoNotifier::*member_func)(TimerBase*))
        : timer_(web_task_runner, notifier, member_func), notifier_(notifier) {}

    void Trace(blink::Visitor*);

    // TimerBase-compatible API
    void StartOneShot(base::TimeDelta interval, const base::Location& caller);
    void Stop();
    bool IsActive() const { return timer_.IsActive(); }

   private:
    TaskRunnerTimer<GeoNotifier> timer_;
    Member<GeoNotifier> notifier_;
  };

  // Runs the error callback if there is a fatal error. Otherwise, if a
  // cached position must be used, registers itself for receiving one.
  // Otherwise, the notifier has expired, and its error callback is run.
  void TimerFired(TimerBase*);

  Member<Geolocation> geolocation_;
  Member<V8PositionCallback> success_callback_;
  Member<V8PositionErrorCallback> error_callback_;
  Member<const PositionOptions> options_;
  Member<Timer> timer_;
  Member<GeolocationPositionError> fatal_error_;
  bool use_cached_position_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_GEOLOCATION_GEO_NOTIFIER_H_
