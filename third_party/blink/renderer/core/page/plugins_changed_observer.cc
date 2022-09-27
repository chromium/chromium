// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/plugins_changed_observer.h"

#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

PluginsChangedObserver::PluginsChangedObserver(Page* page) {
  if (page)
    page->RegisterPluginsChangedObserver(this);
}

}  // namespace blink
