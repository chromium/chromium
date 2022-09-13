// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_EVENT_FILTER_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_EVENT_FILTER_H_

#include <fuchsia/web/cpp/fidl.h>

#include "fuchsia_web/webengine/web_engine_export.h"
#include "ui/events/event_handler.h"
#include "ui/events/types/event_type.h"

// Event filter which can be configured to drop all events, or certain kinds of
// events.
class WEB_ENGINE_EXPORT EventFilter : public ui::EventHandler {
 public:
  EventFilter();
  ~EventFilter() override;

  EventFilter(const EventFilter&) = delete;
  EventFilter& operator=(const EventFilter&) = delete;

  void ConfigureInputTypes(fuchsia::web::InputTypes types,
                           fuchsia::web::AllowInputState allow);

 private:
  friend class EventFilterTest;

  bool IsEventAllowed(ui::EventType type);

  // Returns whether |type| is set in the |enabled_input_types_| bitmask.
  bool IsTypeEnabled(fuchsia::web::InputTypes type) const;

  // ui::EventRewriter implementation.
  void OnEvent(ui::Event* event) final;
  void OnGestureEvent(ui::GestureEvent* event) final;

  uint64_t enabled_input_types_ = 0;

  // Allows input events not mapped to fuchsia::web::InputTypes entries
  // to be processed. Set by allowing or denying fuchsia::web::InputTypes::ALL.
  bool enable_unknown_types_ = true;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_EVENT_FILTER_H_
