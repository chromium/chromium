// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zoom/test/zoom_test_utils.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "content/public/test/test_utils.h"
#include "third_party/blink/public/common/page/page_zoom.h"

namespace zoom {

bool operator==(const ZoomController::ZoomChangedEventData& lhs,
                const ZoomController::ZoomChangedEventData& rhs) {
  return lhs.web_contents == rhs.web_contents &&
         blink::ZoomValuesEqual(lhs.old_zoom_level, rhs.old_zoom_level) &&
         blink::ZoomValuesEqual(lhs.new_zoom_level, rhs.new_zoom_level) &&
         lhs.zoom_mode == rhs.zoom_mode &&
         lhs.can_show_bubble == rhs.can_show_bubble;
}

namespace {

// Create a predicate for checking equality with the given expected event data.
ZoomChangedWatcher::ZoomEventPred ZoomChangeEqualTo(
    const ZoomController::ZoomChangedEventData& expected) {
  return base::BindRepeating(
      [](const ZoomController::ZoomChangedEventData& lhs,
         const ZoomController::ZoomChangedEventData& rhs) {
        return lhs == rhs;
      },
      expected);
}

}  // namespace

ZoomChangedWatcher::ZoomChangedWatcher(ZoomController* zoom_controller,
                                       ZoomEventPred predicate)
    : zoom_controller_(zoom_controller),
      predicate_(predicate),
      message_loop_runner_(new content::MessageLoopRunner) {
  DCHECK(zoom_controller_);
  zoom_observation_.Observe(zoom_controller_);
}

ZoomChangedWatcher::ZoomChangedWatcher(content::WebContents* web_contents,
                                       ZoomEventPred predicate)
    : ZoomChangedWatcher(ZoomController::FromWebContents(web_contents),
                         predicate) {}

ZoomChangedWatcher::ZoomChangedWatcher(
    ZoomController* zoom_controller,
    const ZoomController::ZoomChangedEventData& expected_event_data)
    : ZoomChangedWatcher(zoom_controller,
                         ZoomChangeEqualTo(expected_event_data)) {}

ZoomChangedWatcher::ZoomChangedWatcher(
    content::WebContents* web_contents,
    const ZoomController::ZoomChangedEventData& expected_event_data)
    : ZoomChangedWatcher(web_contents, ZoomChangeEqualTo(expected_event_data)) {
}

ZoomChangedWatcher::~ZoomChangedWatcher() {
  zoom_controller_->RemoveObserver(this);
}

void ZoomChangedWatcher::Wait() {
  if (!change_received_)
    message_loop_runner_->Run();
}

void ZoomChangedWatcher::OnZoomControllerDestroyed(
    zoom::ZoomController* zoom_controller) {
  zoom_observation_.Reset();
}

void ZoomChangedWatcher::OnZoomChanged(
    const ZoomController::ZoomChangedEventData& event_data) {
  DCHECK_EQ(zoom_controller_->web_contents(), event_data.web_contents);
  if (predicate_.Run(event_data)) {
    change_received_ = true;
    if (message_loop_runner_->loop_running())
      message_loop_runner_->Quit();
  }
}

}  // namespace zoom
