// Copyright 2023 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/record_replay.h"
#include "third_party/blink/renderer/bindings/core/v8/record_replay_events.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

using namespace blink;

namespace recordreplay {
/** ###########################################################################
 * User event probing and handling
 * ##########################################################################*/

static int gIsEventInFlight = 0;

// This gathers and encodes the data points necessary for the "event breakpoint
// matching logic" of CDT. Based on DOMDebuggerModel:
// https://chromium.googlesource.com/devtools/devtools-frontend/+/3a80260722c77d984a637b923cad4883857e57dc/front_end/core/sdk/DOMDebuggerModel.ts#L958
static String MakeReplayEventType(const String& eventTypeRaw,
                                  EventTarget* eventTarget,
                                  bool isCallback) {
  // build final name
  StringBuilder builder;
  builder.Append(eventTypeRaw);
  if (isCallback) {
    builder.Append(".callback");
  }

  if (eventTarget) {
    // Sadly, this "string assembling" is necessary because the lookup logic needs to distinguish
    // between event and target name:
    // The event name itself is ambiguous. To resolve it, sometimes the target
    // name needs to be used, sometimes it needs to be omitted.
    // Thus, we need to keep them logically separate.
    Node* node = eventTarget->ToNode();
    auto targetName = node ? node->nodeName() : eventTarget->InterfaceName();
    builder.Append(",");
    builder.Append(targetName);
  }

  return builder.ToString();
}

// Call `ReplayOnEvent` for the given event *before* it happened.
static void ReplayNotifyBeforeEvent(const String& eventName,
                                    EventTarget* eventTarget = nullptr,
                                    bool isCallback = false);

// Call `ReplayOnEvent` for the given event *after* it happened.
static void ReplayNotifyAfterEvent(const String& eventName,
                                   EventTarget* eventTarget = nullptr,
                                   bool isCallback = false);

bool ShouldNotifyEvent(const String& eventName) {
  return !AreEventsDisallowed() &&
         // check for events feature flag (RUN-1609)
         IsRecordingOrReplaying("collect-events") &&
         // Main-thread only (RUN-1392)
         IsMainThread() &&
         !eventName.empty();
}

void ReplayNotifyBeforeEvent(const String& eventName,
                             EventTarget* eventTarget,
                             bool isCallback) {
  if (ShouldNotifyEvent(eventName)) {
    String replayEventType =
        MakeReplayEventType(eventName, eventTarget, isCallback);
    REPLAY_ASSERT("[RUN-1271] ReplayNotifyBeforeEvent %d %s", gIsEventInFlight,
           replayEventType.Ascii().c_str());
    OnEvent(replayEventType.Ascii().c_str(), true);
    ++gIsEventInFlight;
  }
}

void ReplayNotifyAfterEvent(const String& eventName,
                            EventTarget* eventTarget,
                            bool isCallback) {
  if (ShouldNotifyEvent(eventName)) {
    String replayEventType =
        MakeReplayEventType(eventName, eventTarget, isCallback);
    REPLAY_ASSERT("[RUN-1271] ReplayNotifyAfterEvent %d %s", gIsEventInFlight,
           replayEventType.Ascii().c_str());
    OnEvent(replayEventType.Ascii().c_str(), false);
    --gIsEventInFlight;
  }
}

UserEventProbe::UserEventProbe(const char* name,
                               const AtomicString& atomic_name,
                               EventTarget* event_target,
                               bool is_callback)
    : name_(name ? String(name) : atomic_name),
      event_target_(event_target),
      is_callback_(is_callback) {
  ReplayNotifyBeforeEvent(name_, event_target_, is_callback);
}

UserEventProbe::UserEventProbe(const char* name,
                               const AtomicString& atomic_name,
                               EventTarget* event_target)
    : UserEventProbe(name, atomic_name, event_target, !event_target) {}

// NOTE: This is used for capturing script run events (`scriptFirstStatement`)
// but it does not work, possibly because it runs too early or otherwise
// interferes with our own script runners. Not sure yet.
// TODO: debug this - https://linear.app/replay/issue/RUN-1271#comment-293243fc
UserEventProbe::UserEventProbe()
{}

UserEventProbe::~UserEventProbe() {
  ReplayNotifyAfterEvent(name_, event_target_, is_callback_);
}

}  // namespace recordreplay
