// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/guest_view_events.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "extensions/browser/guest_view/extension_options/extension_options_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/common/api/extension_options_internal.h"

namespace extensions {
namespace guest_view_events {

namespace {

class EventMap {
 public:
  EventMap() {
    struct NameAndValue {
      const char* name;
      events::HistogramValue value;
    } names_and_values[] = {
        {webview::kEventContextMenuShow,
         events::CHROME_WEB_VIEW_INTERNAL_ON_CONTEXT_MENU_SHOW},
        {api::extension_options_internal::OnClose::kEventName,
         events::EXTENSION_OPTIONS_INTERNAL_ON_CLOSE},
        {api::extension_options_internal::OnLoad::kEventName,
         events::EXTENSION_OPTIONS_INTERNAL_ON_LOAD},
        {api::extension_options_internal::OnPreferredSizeChanged::kEventName,
         events::EXTENSION_OPTIONS_INTERNAL_ON_PREFERRED_SIZE_CHANGED},
        {guest_view::kEventResize, events::GUEST_VIEW_INTERNAL_ON_RESIZE},
        {webview::kEventAudioStateChanged,
         events::WEB_VIEW_INTERNAL_ON_AUDIO_STATE_CHANGED},
        {webview::kEventBeforeRequest,
         events::WEB_VIEW_INTERNAL_ON_BEFORE_REQUEST},
        {webview::kEventBeforeSendHeaders,
         events::WEB_VIEW_INTERNAL_ON_BEFORE_SEND_HEADERS},
        {webview::kEventClose, events::WEB_VIEW_INTERNAL_ON_CLOSE},
        {webview::kEventCompleted, events::WEB_VIEW_INTERNAL_ON_COMPLETED},
        {webview::kEventConsoleMessage,
         events::WEB_VIEW_INTERNAL_ON_CONSOLE_MESSAGE},
        {webview::kEventContentLoad, events::WEB_VIEW_INTERNAL_ON_CONTENT_LOAD},
        {webview::kEventDialog, events::WEB_VIEW_INTERNAL_ON_DIALOG},
        {webview::kEventDropLink, events::WEB_VIEW_INTERNAL_ON_DROP_LINK},
        {webview::kEventExit, events::WEB_VIEW_INTERNAL_ON_EXIT},
        {webview::kEventExitFullscreen,
         events::WEB_VIEW_INTERNAL_ON_EXIT_FULLSCREEN},
        {webview::kEventFindReply, events::WEB_VIEW_INTERNAL_ON_FIND_REPLY},
        {webview::kEventHeadersReceived,
         events::WEB_VIEW_INTERNAL_ON_HEADERS_RECEIVED},
        {webview::kEventFrameNameChanged,
         events::WEB_VIEW_INTERNAL_ON_FRAME_NAME_CHANGED},
        {webview::kEventLoadAbort, events::WEB_VIEW_INTERNAL_ON_LOAD_ABORT},
        {webview::kEventLoadCommit, events::WEB_VIEW_INTERNAL_ON_LOAD_COMMIT},
        {webview::kEventLoadProgress,
         events::WEB_VIEW_INTERNAL_ON_LOAD_PROGRESS},
        {webview::kEventLoadRedirect,
         events::WEB_VIEW_INTERNAL_ON_LOAD_REDIRECT},
        {webview::kEventLoadStart, events::WEB_VIEW_INTERNAL_ON_LOAD_START},
        {webview::kEventLoadStop, events::WEB_VIEW_INTERNAL_ON_LOAD_STOP},
        {webview::kEventNewWindow, events::WEB_VIEW_INTERNAL_ON_NEW_WINDOW},
        {webview::kEventPermissionRequest,
         events::WEB_VIEW_INTERNAL_ON_PERMISSION_REQUEST},
        {webview::kEventResponseStarted,
         events::WEB_VIEW_INTERNAL_ON_RESPONSE_STARTED},
        {webview::kEventResponsive, events::WEB_VIEW_INTERNAL_ON_RESPONSIVE},
        {webview::kEventSizeChanged, events::WEB_VIEW_INTERNAL_ON_SIZE_CHANGED},
        {webview::kEventUnresponsive,
         events::WEB_VIEW_INTERNAL_ON_UNRESPONSIVE},
        {webview::kEventZoomChange, events::WEB_VIEW_INTERNAL_ON_ZOOM_CHANGE},
        {webview::kEventAuthRequired,
         events::WEB_VIEW_INTERNAL_ON_AUTH_REQUIRED},
        {webview::kEventBeforeRedirect,
         events::WEB_VIEW_INTERNAL_ON_BEFORE_REDIRECT},
        {webview::kEventErrorOccurred,
         events::WEB_VIEW_INTERNAL_ON_ERROR_OCCURRED},
        {webview::kEventSendHeaders, events::WEB_VIEW_INTERNAL_ON_SEND_HEADERS},
    };
    for (const auto& name_and_value : names_and_values) {
      values_[name_and_value.name] = name_and_value.value;
    }
  }

  events::HistogramValue Get(const std::string& event_name) {
    auto value = values_.find(event_name);
    return value != values_.end() ? value->second : events::UNKNOWN;
  }

 private:
  std::map<std::string, events::HistogramValue> values_;

  DISALLOW_COPY_AND_ASSIGN(EventMap);
};

base::LazyInstance<EventMap>::DestructorAtExit g_event_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

events::HistogramValue GetEventHistogramValue(const std::string& event_name) {
  return g_event_map.Get().Get(event_name);
}

}  // namespace guest_view_events
}  // namespace extensions
