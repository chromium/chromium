// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AX_INSPECT_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_AX_INSPECT_FACTORY_H_

#include <memory>

#include "content/common/content_export.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter.h"

namespace content {

// Accessibility tree formatters and event recorders factory.
class CONTENT_EXPORT AXInspectFactory {
 public:
  // Creates the appropriate tree formatter for the platform we are currently
  // running on, since each platform has its own specific accessibility tree.
  // For example, this would be MSAA/IAccessible2 tree on Windows, AT-SPI tree
  // on Linux or NSAccessibility tree on macOS.
  // TODO(crbug.com/1133330): CreateDefaultFormatter method for each platform
  // are implemented in conrresponding AccessibilityTreeFormatter, for example,
  // macOS version is implemented in
  // content/browser/accessibility/accessibility_tree_formatter_mac.h file.
  // All implementation should be moved into ax_inspect_factory.cc eventually
  // when tree formatters are moved under ui/accessibility/platform umbrella.
  static std::unique_ptr<ui::AXTreeFormatter> CreatePlatformFormatter();

  // Creates the internal accessibility tree formatter, AKA the Blink tree
  // formatter, which is used to dump the Blink accessibility tree to a string
  static std::unique_ptr<ui::AXTreeFormatter> CreateBlinkFormatter();

  // Inspect types for all platforms.
  enum TypeConstant {
    kAndroid,
    kBlink,
    kMac,
    kLinux,
    kWinIA2,
    kWinUIA,
  };

  // Inspect type.
  class CONTENT_EXPORT Type final {
   public:
    Type(TypeConstant type) : type_(type) {}

    explicit operator std::string() const;
    operator TypeConstant() const { return type_; }

   private:
    TypeConstant type_;
  };

  // Creates a tree formatter of a given inspect type if supported by platform.
  static std::unique_ptr<ui::AXTreeFormatter> CreateFormatter(Type);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_AX_INSPECT_FACTORY_H_
