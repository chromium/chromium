// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LCP_CRITICAL_PATH_PREDICTOR_ELEMENT_LOCATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LCP_CRITICAL_PATH_PREDICTOR_ELEMENT_LOCATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Element;

CORE_EXPORT String ToElementLocatorString(Element*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_H_
