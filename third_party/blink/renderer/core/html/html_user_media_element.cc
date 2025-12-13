// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/html/html_user_media_element.h"

#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;

HTMLUserMediaElement::HTMLUserMediaElement(Document& document)
    : HTMLPermissionElement(document, html_names::kUsermediaTag) {
  CHECK(RuntimeEnabledFeatures::UserMediaElementEnabled(
      document.GetExecutionContext()));
}

Vector<PermissionDescriptorPtr> HTMLUserMediaElement::ParseType(
    const AtomicString& type) {
  Vector<PermissionDescriptorPtr> permission_descriptors =
      HTMLPermissionElement::ParseType(type);

  // camera/microphone are the only allowed descriptors.
  for (const auto& descriptor : permission_descriptors) {
    if (descriptor->name != PermissionName::VIDEO_CAPTURE &&
        descriptor->name != PermissionName::AUDIO_CAPTURE) {
      return Vector<PermissionDescriptorPtr>();
    }
  }

  return permission_descriptors;
}

}  // namespace blink
