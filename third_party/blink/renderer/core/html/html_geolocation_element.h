// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_GEOLOCATION_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_GEOLOCATION_ELEMENT_H_

#include "base/time/time.h"
#include "base/types/expected.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/geolocation/geolocation.h"
#include "third_party/blink/renderer/core/geolocation/geolocation_position_error.h"
#include "third_party/blink/renderer/core/geolocation/geolocation_watchers.h"
#include "third_party/blink/renderer/core/geolocation/geoposition.h"
#include "third_party/blink/renderer/core/html/html_permission_element.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class CORE_EXPORT HTMLGeolocationElement final : public HTMLPermissionElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLGeolocationElement(Document&);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(location, kLocation)

  bool precise() const { return precise_; }
  void setPrecise(bool value) { precise_ = value; }

  bool autolocate() const { return autolocate_; }
  void setAutolocate(bool value) { autolocate_ = value; }

  bool watch() const { return watch_; }
  void setWatch(bool value) { watch_ = value; }

  Geoposition* position() const;
  GeolocationPositionError* error() const;

  void Trace(Visitor*) const override;

  HeapTaskRunnerTimer<HTMLGeolocationElement>& SpinningIconTimerForTesting() {
    return spinning_icon_timer_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest,
                           GeolocationUsingLocationAppearance);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest,
                           GeolocationTranslateInnerText);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest,
                           GeolocationWatchPositionAppearance);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest,
                           GeolocationGrantedClickBehavior);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest, GeolocationAutolocate);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest,
                           GeolocationAutolocateWatch);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest,
                           GeolocationAutolocateTriggersOnce);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest,
                           GeolocationTranslateInnerText);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest,
                           RequestLocationAfterClickAndPermissionChanged);

  // HTMLPermissionElement:
  void UpdateAppearance() override;
  void UpdatePermissionStatusAndAppearance() override;
  mojom::blink::EmbeddedPermissionRequestDescriptorPtr
  CreateEmbeddedPermissionRequestDescriptor() override;
  void AttributeChanged(const AttributeModificationParams& params) override;
  void DefaultEventHandler(Event&) override;
  void OnPermissionStatusChange(mojom::blink::PermissionName,
                                mojom::blink::PermissionStatus) override;
  void DidFinishLifecycleUpdate(const LocalFrameView&) override;

  void OnActivated();
  void GetCurrentPosition();
  void WatchPosition();
  // Callback for Geolocation::getCurrentPosition. It is called when the
  // geolocation API returns a position or an error.
  void CurrentPositionCallback(
      base::expected<Geoposition*, GeolocationPositionError*>);
  Geolocation* GetGeolocation();
  void SpinningIconTimerFired(TimerBase*);
  void MaybeStopSpinning();
  enum class RequestInProgress { kNo, kYes };
  void StartSpinning(RequestInProgress request_in_progress);
  bool ShouldShowSpinningIcon();
  void RequestGeolocation();
  void ClearWatch();
  enum class ForceAutolocate { kNo, kYes };
  void MaybeTriggerAutolocate(ForceAutolocate);
  void UpdateText();

  bool precise_ = false;
  bool autolocate_ = false;
  bool watch_ = false;
  // The watch_id_ is used to identify the watcher in the Geolocation object.
  // The ids always start from 1. 0 means that the watch is not set.
  int watch_id_ = 0;
  bool did_autolocate_trigger_request = false;
  bool is_geolocation_request_in_progress_ = false;
  base::TimeTicks spinning_started_time_;
  HeapTaskRunnerTimer<HTMLGeolocationElement> spinning_icon_timer_;

  Member<Geoposition> position_;
  Member<GeolocationPositionError> error_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_GEOLOCATION_ELEMENT_H_
