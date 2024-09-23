// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_ICON_MANAGER_H_
#define EXTENSIONS_BROWSER_EXTENSION_ICON_MANAGER_H_

#include <map>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/extension_id.h"
#include "ui/gfx/image/image.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;

class ExtensionIconManager {
 public:
  class Observer {
   public:
    virtual void OnImageLoaded(const ExtensionId& extension_id) = 0;
  };

  ExtensionIconManager();

  ExtensionIconManager(const ExtensionIconManager&) = delete;
  ExtensionIconManager& operator=(const ExtensionIconManager&) = delete;

  virtual ~ExtensionIconManager();

  // Start loading the icon for the given extension.
  void LoadIcon(content::BrowserContext* context, const Extension* extension);

  // This returns an image of width/height kFaviconSize, loaded either from an
  // entry specified in the extension's 'icon' section of the manifest, or a
  // default extension icon.
  gfx::Image GetIcon(const ExtensionId& extension_id);

  // Removes the extension's icon from memory.
  void RemoveIcon(const ExtensionId& extension_id);

  void set_monochrome(bool value) { monochrome_ = value; }
  void set_observer(Observer* observer) { observer_ = observer; }

 private:
  void OnImageLoaded(const ExtensionId& extension_id, const gfx::Image& image);

  // Makes sure we've done one-time initialization of the default extension icon
  // default_icon_.
  void EnsureDefaultIcon();

  // Maps extension id to the icon for that extension.
  std::map<ExtensionId, gfx::Image> icons_;

  // Set of extension IDs waiting for icons to load.
  std::set<ExtensionId> pending_icons_;

  // The default icon we'll use if an extension doesn't have one.
  gfx::Image default_icon_;

  // If true, we will desaturate the icons to make them monochromatic.
  bool monochrome_ = false;

  raw_ptr<Observer> observer_ = nullptr;

  base::WeakPtrFactory<ExtensionIconManager> weak_ptr_factory_{this};
};

} // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_ICON_MANAGER_H_
