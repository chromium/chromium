// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZOOM_TEST_ZOOM_TEST_UTILS_H_
#define COMPONENTS_ZOOM_TEST_ZOOM_TEST_UTILS_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/zoom/zoom_controller.h"
#include "components/zoom/zoom_observer.h"

namespace content {
class MessageLoopRunner;
}

namespace zoom {

bool operator==(const ZoomController::ZoomChangedEventData& lhs,
                const ZoomController::ZoomChangedEventData& rhs);

class ZoomChangedWatcher : public zoom::ZoomObserver {
 public:
  using ZoomEventPred = base::RepeatingCallback<bool(
      const ZoomController::ZoomChangedEventData&)>;

  // Used to wait until we see a zoom changed event that satisfies the
  // given |predicate|.
  ZoomChangedWatcher(ZoomController* zoom_controller, ZoomEventPred predicate);
  ZoomChangedWatcher(content::WebContents* web_contents,
                     ZoomEventPred predicate);

  // Used to wait until we see a zoom changed event equal to the given
  // |expected_event_data|.
  ZoomChangedWatcher(
      ZoomController* zoom_controller,
      const ZoomController::ZoomChangedEventData& expected_event_data);
  ZoomChangedWatcher(
      content::WebContents* web_contents,
      const ZoomController::ZoomChangedEventData& expected_event_data);

  ZoomChangedWatcher(const ZoomChangedWatcher&) = delete;
  ZoomChangedWatcher& operator=(const ZoomChangedWatcher&) = delete;

  ~ZoomChangedWatcher() override;

  void Wait();

  // zoom::ZoomObserver:
  void OnZoomControllerDestroyed(
      zoom::ZoomController* zoom_controller) override;
  void OnZoomChanged(
      const ZoomController::ZoomChangedEventData& event_data) override;

 private:
  raw_ptr<ZoomController> zoom_controller_;
  ZoomEventPred predicate_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  bool change_received_ = false;
  base::ScopedObservation<zoom::ZoomController, zoom::ZoomObserver>
      zoom_observation_{this};
};

}  // namespace zoom
#endif  // COMPONENTS_ZOOM_TEST_ZOOM_TEST_UTILS_H_
