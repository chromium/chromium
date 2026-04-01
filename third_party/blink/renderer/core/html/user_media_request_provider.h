// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_USER_MEDIA_REQUEST_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_USER_MEDIA_REQUEST_PROVIDER_H_

#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class HTMLUserMediaElement;
class LocalDOMWindow;

class CORE_EXPORT UserMediaRequestProvider
    : public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  static UserMediaRequestProvider* From(LocalDOMWindow&);

  virtual void StartRequest(
      HTMLUserMediaElement*,
      const Vector<mojom::blink::PermissionDescriptorPtr>&) = 0;

  void Trace(Visitor*) const override;

 protected:
  explicit UserMediaRequestProvider(LocalDOMWindow&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_USER_MEDIA_REQUEST_PROVIDER_H_
