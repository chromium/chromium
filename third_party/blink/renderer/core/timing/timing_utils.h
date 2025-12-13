#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_TIMING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_TIMING_UTILS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class EventTarget;

// Returns a string representation of an EventTarget, suitable for including in
// performance timeline entries.
CORE_EXPORT AtomicString EventTargetToString(EventTarget*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_TIMING_UTILS_H_
