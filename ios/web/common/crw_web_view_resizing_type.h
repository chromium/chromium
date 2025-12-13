// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_CRW_WEB_VIEW_RESIZING_TYPE_H_
#define IOS_WEB_COMMON_CRW_WEB_VIEW_RESIZING_TYPE_H_

// The different types of web view resizing strategies.
enum class WebViewResizingType {
  // Default behavior, adjusts the web view's scroll view content inset.
  // Starting from iOS 26, this also update the obsured inset which is a portion
  // of the web view that is obscured by other UI like toolbars.
  // This is the most performant option.
  kContentInset,
  // Resizes the web view's frame. Used for web pages with websites with
  // elements that don't work well with the contentInset strategy.
  kFrame,
};

#endif  // IOS_WEB_COMMON_CRW_WEB_VIEW_RESIZING_TYPE_H_
