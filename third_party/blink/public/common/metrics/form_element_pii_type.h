// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_METRICS_FORM_ELEMENT_PII_TYPE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_METRICS_FORM_ELEMENT_PII_TYPE_H_

#include <stdint.h>

namespace blink {

// PII (i.e. Personally identifiable information) type of html form element.
enum class FormElementPiiType {
  kUnknown,

  kEmail,
  kPhone,

  // It's some PII type, but we are currently not interested in the specific
  // category.
  kOthers,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_METRICS_FORM_ELEMENT_PII_TYPE_H_
