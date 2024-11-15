// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/tracing_delegate.h"

#include <optional>

#include "base/values.h"

namespace content {

bool TracingDelegate::IsRecordingAllowed(bool requires_anonymized_data) const {
  return false;
}

bool TracingDelegate::ShouldSaveUnuploadedTrace() const {
  return false;
}

}  // namespace content
