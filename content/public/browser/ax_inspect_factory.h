// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AX_INSPECT_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_AX_INSPECT_FACTORY_H_

#include <memory>

#include "base/process/process_handle.h"
#include "content/common/content_export.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter.h"

namespace content {

// TODO: we shouldn't leak internal data types outside of the content module,
// event recorders can use native platform APIs and avoid dealing
// with BrowserAccessibilityManager, see crbug.com/1133330.
class BrowserAccessibilityManager;

// Accessibility tree formatters and event recorders factory.
class CONTENT_EXPORT AXInspectFactory {
 public:
  // Creates the appropriate tree formatter for the platform we are currently
  // running on, since each platform has its own specific accessibility tree.
  // For example, this would be MSAA/IAccessible2 tree on Windows, AT-SPI tree
  // on Linux or NSAccessibility tree on macOS.
  static std::unique_ptr<ui::AXTreeFormatter> CreatePlatformFormatter();

  // Creates the appropriate event recorder for the platform we are currently
  // running on.
  static std::unique_ptr<ui::AXEventRecorder> CreatePlatformRecorder(
      BrowserAccessibilityManager* manager = nullptr,
      base::ProcessId pid = 0,
      const ui::AXTreeSelector& selector = {});

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

  // Creates an event recorder of a given inspect type if supported by platform.
  static std::unique_ptr<ui::AXEventRecorder> CreateRecorder(
      Type,
      BrowserAccessibilityManager* manager = nullptr,
      base::ProcessId pid = 0,
      const ui::AXTreeSelector& selector = {});
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_AX_INSPECT_FACTORY_H_
