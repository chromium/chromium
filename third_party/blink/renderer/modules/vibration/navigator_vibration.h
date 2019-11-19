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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_VIBRATION_NAVIGATOR_VIBRATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_VIBRATION_NAVIGATOR_VIBRATION_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LocalFrame;
class Navigator;
class VibrationController;

enum NavigatorVibrationType {
  kMainFrameNoUserGesture = 0,
  kMainFrameWithUserGesture = 1,
  kSameOriginSubFrameNoUserGesture = 2,
  kSameOriginSubFrameWithUserGesture = 3,
  kCrossOriginSubFrameNoUserGesture = 4,
  kCrossOriginSubFrameWithUserGesture = 5,
  kEnumMax = 6
};

class MODULES_EXPORT NavigatorVibration final
    : public GarbageCollected<NavigatorVibration>,
      public Supplement<Navigator>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorVibration);

 public:
  static const char kSupplementName[];

  using VibrationPattern = Vector<unsigned>;

  explicit NavigatorVibration(Navigator&);
  virtual ~NavigatorVibration();

  static NavigatorVibration& From(Navigator&);

  static bool vibrate(Navigator&, unsigned time);
  static bool vibrate(Navigator&, const VibrationPattern&);

  VibrationController* Controller(LocalFrame&);

  void Trace(blink::Visitor*) override;

 private:
  // Inherited from ContextLifecycleObserver.
  void ContextDestroyed(ExecutionContext*) override;

  static void CollectHistogramMetrics(const Navigator&);

  Member<VibrationController> controller_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorVibration);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_VIBRATION_NAVIGATOR_VIBRATION_H_
