// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATE_OBSERVER_H_
#define EXTENSIONS_BROWSER_UPDATE_OBSERVER_H_

namespace extensions {
class Extension;

class UpdateObserver {
 public:
  // Invoked when an app update is available.
  virtual void OnAppUpdateAvailable(const Extension* extension) = 0;

  // Invoked when Chrome update is available.
  virtual void OnChromeUpdateAvailable() = 0;

 protected:
  virtual ~UpdateObserver() {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATE_OBSERVER_H_
