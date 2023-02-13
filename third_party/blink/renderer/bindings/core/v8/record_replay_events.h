#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_RECORD_REPLAY_EVENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_RECORD_REPLAY_EVENTS_H_

#include "base/values.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace recordreplay {

// Emulate probe::UserCallback (but without InspectorDOMDebuggerAgent).
struct UserEventProbe {
  String name_;
  blink::EventTarget* event_target_;
  bool is_callback_;

  UserEventProbe(const char* name,
                 const AtomicString& atomic_name,
                 blink::EventTarget* event_target,
                 bool is_callback);

  // Used for probes::UserCallback.
  // see: https://source.chromium.org/chromium/chromium/src/+/main:out/android-Debug/gen/third_party/blink/renderer/core/core_probes_impl.cc;l=2090;drc=0c4306fc554c80506eb0f9b833a5d2a5fdd452d5;bpv=1;bpt=1
  UserEventProbe(const char* name,
                 const AtomicString& atomic_name,
                 blink::EventTarget* event_target = nullptr);

  // Primarily used for probes::ExecuteScript.
  // see: https://source.chromium.org/search?q=%22ExecuteScript%20probe%22&sq=&ss=chromium%2Fchromium%2Fsrc
  UserEventProbe();

  ~UserEventProbe();
};

}  // namespace recordreplay

#endif // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_RECORD_REPLAY_EVENTS_H_
