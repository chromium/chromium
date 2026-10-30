// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_INPUT_ITEM_SOURCE_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_INPUT_ITEM_SOURCE_H_

// Enum representing the source of an item in the composebox.
enum class ComposeboxInputItemSource {
  kUnknown = 0,
  kGalleryPicker,
  kCameraPicker,
  kFilePicker,
  kTabPicker,
  kDragAndDrop,
  kCurrentTab,
  kDrivePicker,
};

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_INPUT_ITEM_SOURCE_H_
