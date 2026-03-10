#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_TIMING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_TIMING_UTILS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class EventTarget;
class EventPath;

// Returns a string representation of an EventTarget, suitable for including in
// performance timeline entries.
// This may be used for non-UI events and is currently used by the Long
// Animation Frames (LoAF) API.
CORE_EXPORT AtomicString EventTargetToString(EventTarget*);

// Returns a string representation of an EventPath (CSS selector style).
// This is primarily intended for UI events where the propagation path
// provides meaningful context about the interaction.
CORE_EXPORT AtomicString EventPathToSelector(const EventPath&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_TIMING_UTILS_H_
