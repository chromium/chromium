// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_inspect_factory.h"

#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"

namespace content {

std::unique_ptr<ui::AXTreeFormatter> AXInspectFactory::CreateBlinkFormatter() {
  return CreateFormatter(kBlink);
}

AXInspectFactory::Type::operator std::string() const {
  switch (type_) {
    case kAndroid:
      return "android";
    case kBlink:
      return "blink";
    case kMac:
      return "mac";
    case kLinux:
      return "linux";
    case kWinIA2:
      return "win";
    case kWinUIA:
      return "uia";
    default:
      return "unknown";
  }
}

}  // namespace content
