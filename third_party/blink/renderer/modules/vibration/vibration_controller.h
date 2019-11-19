/*
 *  Copyright (C) 2012 Samsung Electronics
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_VIBRATION_VIBRATION_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_VIBRATION_VIBRATION_CONTROLLER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/vibration_manager.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LocalFrame;
class UnsignedLongOrUnsignedLongSequence;

class MODULES_EXPORT VibrationController final
    : public GarbageCollected<VibrationController>,
      public ContextLifecycleObserver,
      public PageVisibilityObserver {
  USING_GARBAGE_COLLECTED_MIXIN(VibrationController);

 public:
  using VibrationPattern = Vector<unsigned>;

  explicit VibrationController(LocalFrame&);
  virtual ~VibrationController();

  static VibrationPattern SanitizeVibrationPattern(
      const UnsignedLongOrUnsignedLongSequence&);

  bool Vibrate(const VibrationPattern&);
  void DoVibrate(TimerBase*);
  void DidVibrate();

  // Cancels the ongoing vibration if there is one.
  void Cancel();
  void DidCancel();

  // Whether a pattern is being processed. If this is true, the vibration
  // hardware may currently be active, but during a pause it may be inactive.
  bool IsRunning() const { return is_running_; }

  VibrationPattern Pattern() const { return pattern_; }

  void Trace(blink::Visitor*) override;

 private:
  // Inherited from ContextLifecycleObserver.
  void ContextDestroyed(ExecutionContext*) override;

  // Inherited from PageVisibilityObserver.
  void PageVisibilityChanged() override;

  // Remote to VibrationManager mojo interface. This is reset in
  // |contextDestroyed| and must not be called or recreated after it is reset.
  mojo::Remote<device::mojom::blink::VibrationManager> vibration_manager_;

  // Timer for calling |doVibrate| after a delay. It is safe to call
  // |startOneshot| when the timer is already running: it may affect the time
  // at which it fires, but |doVibrate| will still be called only once.
  TaskRunnerTimer<VibrationController> timer_do_vibrate_;

  // Whether a pattern is being processed. The vibration hardware may
  // currently be active, or during a pause it may be inactive.
  bool is_running_;

  // Whether an async mojo call to cancel is pending.
  bool is_calling_cancel_;

  // Whether an async mojo call to vibrate is pending.
  bool is_calling_vibrate_;

  VibrationPattern pattern_;

  DISALLOW_COPY_AND_ASSIGN(VibrationController);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_VIBRATION_VIBRATION_CONTROLLER_H_
