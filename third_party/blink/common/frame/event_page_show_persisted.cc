// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/event_page_show_persisted.h"

#include "base/metrics/histogram_macros.h"

namespace blink {

void RecordUMAEventPageShowPersisted(EventPageShowPersisted value) {
  UMA_HISTOGRAM_ENUMERATION("Event.PageShow.Persisted", value);
}

}  // namespace blink
