// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_GEOLOCATION_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_GEOLOCATION_ELEMENT_H_

#include "base/types/expected.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/geolocation/geolocation.h"
#include "third_party/blink/renderer/core/geolocation/geolocation_position_error.h"
#include "third_party/blink/renderer/core/geolocation/geolocation_watchers.h"
#include "third_party/blink/renderer/core/geolocation/geoposition.h"
#include "third_party/blink/renderer/core/html/html_permission_element.h"
#include "third_party/blink/renderer/platform/heap/member.h"

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

  // HTMLPermissionElement:
  void UpdateAppearance() override;
  void UpdatePermissionStatusAndAppearance() override;
  mojom::blink::EmbeddedPermissionRequestDescriptorPtr
  CreateEmbeddedPermissionRequestDescriptor() override;

 private:
  // blink::HTMLPermissionElement:
  void AttributeChanged(const AttributeModificationParams& params) override;
  void GetCurrentPosition();
  void WatchPosition();
  // Callback for Geolocation::getCurrentPosition. It is called when the
  // geolocation API returns a position or an error.
  void CurrentPositionCallback(
      base::expected<Geoposition*, GeolocationPositionError*>);
  Geolocation* GetGeolocation();

  bool precise_ = false;
  bool autolocate_ = false;
  bool watch_ = false;
  // The watch_id_ is used to identify the watcher in the Geolocation object.
  // The ids always start from 1. 0 means that the watch is not set.
  int watch_id_ = 0;

  Member<Geoposition> position_;
  Member<GeolocationPositionError> error_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_GEOLOCATION_ELEMENT_H_
