// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_URL_DOM_ORIGIN_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_URL_DOM_ORIGIN_UTILS_H_

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class LocalDOMWindow;
class DOMOrigin;

class CORE_EXPORT DOMOriginUtils {
 public:
  // Returns a `DOMOrigin` if the window attempting to access the origin passes
  // a security check defined by each class implementing this interface.
  virtual DOMOrigin* GetDOMOrigin(LocalDOMWindow* accessing_window) const = 0;

  virtual ~DOMOriginUtils() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_URL_DOM_ORIGIN_UTILS_H_
