// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_DISPLAY_INFO_PROVIDER_BASE_H_
#define EXTENSIONS_BROWSER_DISPLAY_INFO_PROVIDER_BASE_H_

#include "extensions/browser/api/system_display/display_info_provider.h"

namespace display {
class Screen;
}

namespace extensions {

// This class provides an implementation for the base class pure
// virtual function 'DispatchOnDisplayChangedEvent` that is common
// to all of the derived classes.
class DisplayInfoProviderBase : public DisplayInfoProvider {
 public:
  explicit DisplayInfoProviderBase(display::Screen* screen = nullptr);
  ~DisplayInfoProviderBase() override = 0;

  DisplayInfoProviderBase(const DisplayInfoProviderBase&) = delete;
  DisplayInfoProviderBase& operator=(const DisplayInfoProviderBase&) = delete;

 protected:
  void DispatchOnDisplayChangedEvent() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_DISPLAY_INFO_PROVIDER_BASE_H_
