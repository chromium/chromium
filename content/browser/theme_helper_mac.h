// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_THEME_HELPER_MAC_H_
#define CONTENT_BROWSER_THEME_HELPER_MAC_H_

#include <memory>

#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "third_party/blink/public/common/sandbox_support/sandbox_support_mac.h"
#include "third_party/blink/public/platform/mac/web_scrollbar_theme.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {

// This class is used to monitor macOS system appearance changes and to notify
// sandboxed child processes when they change. This class lives on the UI
// thread.
class ThemeHelperMac : public content::RenderProcessHostCreationObserver {
 public:
  // Return pointer to the singleton instance for the current process, or NULL
  // if none.
  static ThemeHelperMac* GetInstance();

  ThemeHelperMac(const ThemeHelperMac&) = delete;
  ThemeHelperMac& operator=(const ThemeHelperMac&) = delete;

  // Duplicates a handle to the read-only copy of the system color table,
  // which can be shared to sandboxed child processes.
  base::ReadOnlySharedMemoryRegion DuplicateReadOnlyColorMapRegion();

 private:
  ThemeHelperMac();
  ~ThemeHelperMac() override;

  // Looks up the blink::MacSystemColorID corresponding to the NSColor
  // selector and stores them in the |writable_color_map_| table. Looks up
  // colors for both light and dark appearances.
  void LoadSystemColors();

  // Looks up system colors for the current appearance, either a light or
  // dark appearance and stores them in values. The values parameter is the part
  // of writable_color_map_ where the colors for the current appearance should
  // be stored.
  void LoadSystemColorsForCurrentAppearance(base::span<SkColor> values);

  // Overridden from content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

  // Writable and mapped array of SkColor values, indexed by MacSystemColorID
  // for a light appearance. Colors for a dark appearance in indexed by
  // MacSystemColorID starting at index MacSystemColorID::kCount.
  base::WritableSharedMemoryMapping writable_color_map_;

  // Read-only handle to the |writable_color_map_| that can be duplicated for
  // sharing to child processes.
  base::ReadOnlySharedMemoryRegion read_only_color_map_;

  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_THEME_HELPER_MAC_H_
