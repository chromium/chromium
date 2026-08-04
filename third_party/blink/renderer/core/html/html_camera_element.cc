// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_camera_element.h"

#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

using mojom::blink::PermissionName;

HTMLCameraElement::HTMLCameraElement(Document& document)
    : HTMLMediaTrackElementBase(document, html_names::kCameraTag) {
  CHECK(RuntimeEnabledFeatures::CameraAndMicrophoneElementsEnabled(
      document.GetExecutionContext()));
}

void HTMLCameraElement::ApplyDefaultConstraints() {
  if (permission_descriptors_.empty()) {
    permission_descriptors_.push_back(
        CreatePermissionDescriptor(PermissionName::VIDEO_CAPTURE));
  }
  HTMLMediaTrackElementBase::ApplyDefaultConstraints();
}

}  // namespace blink
