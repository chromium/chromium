// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_EVENT_CONVERSION_H_
#define CONTENT_RENDERER_PEPPER_EVENT_CONVERSION_H_

#include <memory>
#include <vector>

#include "content/common/content_export.h"
#include "ppapi/c/ppb_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"

struct PP_InputEvent;

namespace ppapi {
struct InputEventData;
}

namespace blink {
class WebInputEvent;
}

namespace content {

// Converts the given WebKit event to one or possibly multiple PP_InputEvents.
// The generated events will be filled into the given vector. On failure, no
// events will ge generated and the vector will be empty.
CONTENT_EXPORT void CreateInputEventData(
    const blink::WebInputEvent& event,
    std::vector<ppapi::InputEventData>* pp_events);

// Creates a WebInputEvent from the given PP_InputEvent.  If it fails, returns
// NULL.  The caller owns the created object on success.
CONTENT_EXPORT blink::WebInputEvent* CreateWebInputEvent(
    const ppapi::InputEventData& event);

// Creates an array of WebInputEvents to make the given event look like a user
// input event on all platforms. |plugin_x| and |plugin_y| should be the
// coordinates of a point within the plugin's area on the page.
std::vector<std::unique_ptr<blink::WebInputEvent>>
CreateSimulatedWebInputEvents(const ppapi::InputEventData& event,
                              int plugin_x,
                              int plugin_y);

// Returns the PPAPI event class for the given WebKit event type. The given
// type should not be "Undefined" since there's no corresponding PPAPI class.
PP_InputEvent_Class ClassifyInputEvent(const blink::WebInputEvent& event);

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_EVENT_CONVERSION_H_
