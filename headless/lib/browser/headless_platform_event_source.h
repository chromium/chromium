// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_PLATFORM_EVENT_SOURCE_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_PLATFORM_EVENT_SOURCE_H_

#include "ui/events/platform/platform_event_source.h"

namespace headless {

// A stub event source whose main purpose is to prevent the default platform
// event source from getting created.
class HeadlessPlatformEventSource : public ui::PlatformEventSource {
 public:
  HeadlessPlatformEventSource();

  HeadlessPlatformEventSource(const HeadlessPlatformEventSource&) = delete;
  HeadlessPlatformEventSource& operator=(const HeadlessPlatformEventSource&) =
      delete;

  ~HeadlessPlatformEventSource() override;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_PLATFORM_EVENT_SOURCE_H_
