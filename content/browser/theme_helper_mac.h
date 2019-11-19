// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_THEME_HELPER_MAC_H_
#define CONTENT_BROWSER_THEME_HELPER_MAC_H_

#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/blink/public/common/sandbox_support/sandbox_support_mac.h"
#include "third_party/blink/public/platform/mac/web_scrollbar_theme.h"

#if __OBJC__
@class SystemThemeObserver;
#else
class SystemThemeObserver;
#endif

namespace content {

// This class is used to monitor macOS system appearance changes and to notify
// sandboxed child processes when they change. This class lives on the UI
// thread.
class ThemeHelperMac : public NotificationObserver {
 public:
  // Return pointer to the singleton instance for the current process, or NULL
  // if none.
  static ThemeHelperMac* GetInstance();

  // Duplicates a handle to the read-only copy of the system color table,
  // which can be shared to sandboxed child processes.
  base::ReadOnlySharedMemoryRegion DuplicateReadOnlyColorMapRegion();

 private:
  ThemeHelperMac();
  ~ThemeHelperMac() override;

  // Looks up the blink::MacSystemColorID corresponding to the NSColor
  // selector and stores them in the |writable_color_map_| table.
  void LoadSystemColors();

  // Overridden from NotificationObserver:
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  // ObjC object that observes notifications from the system.
  SystemThemeObserver* theme_observer_;  // strong

  // Writable and mapped array of SkColor values, indexed by MacSystemColorID.
  base::WritableSharedMemoryMapping writable_color_map_;

  // Read-only handle to the |writable_color_map_| that can be duplicated for
  // sharing to child processes.
  base::ReadOnlySharedMemoryRegion read_only_color_map_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ThemeHelperMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_THEME_HELPER_MAC_H_
