// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_PUBLIC_OMNIBOX_PRESENTATION_CONTEXT_H_
#define IOS_CHROME_BROWSER_OMNIBOX_PUBLIC_OMNIBOX_PRESENTATION_CONTEXT_H_

// Enum representing the context in which the omnibox is presented.
enum class OmniboxPresentationContext {
  // The omnibox is presented in the location bar.
  kLocationBar,
  // The omnibox is presented in the NTP header view.
  kNTPHeader,
  // The omnibox is presented in the Lens overlay.
  kLensOverlay,
  // The omnibox is presented in the composebox.
  kComposebox,
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_PUBLIC_OMNIBOX_PRESENTATION_CONTEXT_H_
