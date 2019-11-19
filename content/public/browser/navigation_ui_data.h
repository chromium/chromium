// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_UI_DATA_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_UI_DATA_H_

#include <memory>

namespace content {

// Copyable interface for embedders to pass opaque data to content/. It is
// expected to be created on the UI thread at the start of the navigation, and
// content/ will transfer it to the IO thread as a clone.
class NavigationUIData {
 public:
  virtual ~NavigationUIData() {}

  // Creates a new NavigationData that is a deep copy of the original.
  virtual std::unique_ptr<NavigationUIData> Clone() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_UI_DATA_H_
