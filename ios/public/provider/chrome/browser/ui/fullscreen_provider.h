// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UI_FULLSCREEN_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UI_FULLSCREEN_PROVIDER_H_

class FullscreenController;

// Provider class for embedders to provide custom handling of fullscreen scroll
// events.
class FullscreenProvider {
 public:
  FullscreenProvider();
  virtual ~FullscreenProvider();

  // Initializes the provider to respond to scroll events from |controller|.
  virtual void InitializeFullscreen(FullscreenController* controller);

  // Whether or not the provider has been initialized.
  virtual bool IsInitialized() const;
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UI_FULLSCREEN_PROVIDER_H_
