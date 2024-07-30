// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_ACTION_ICON_FACTORY_H_
#define EXTENSIONS_BROWSER_EXTENSION_ACTION_ICON_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "extensions/browser/extension_icon_image.h"

namespace extensions {
class Extension;
class ExtensionAction;

// Used to get an icon to be used in the UI for an extension action.
// If the extension action icon is the default icon defined in the extension's
// manifest, it is loaded using IconImage. This icon can be loaded
// asynchronously. The factory observes underlying IconImage and notifies its
// own observer when the icon image changes.
class ExtensionActionIconFactory : public IconImage::Observer {
 public:
  class Observer {
   public:
    virtual ~Observer() {}
    // Called when the underlying icon image changes.
    virtual void OnIconUpdated() = 0;
  };

  // Observer should outlive this.
  ExtensionActionIconFactory(const Extension* extension,
                             ExtensionAction* action,
                             Observer* observer);

  ExtensionActionIconFactory(const ExtensionActionIconFactory&) = delete;
  ExtensionActionIconFactory& operator=(const ExtensionActionIconFactory&) =
      delete;

  ~ExtensionActionIconFactory() override;

  // Controls whether invisible icons will be returned by GetIcon().
  static void SetAllowInvisibleIconsForTest(bool value);

  // IconImage override.
  void OnExtensionIconImageChanged(IconImage* image) override;
  void OnExtensionIconImageDestroyed(IconImage* image) override;

  // Gets the extension action icon for the tab.
  // If there is an icon set using |SetIcon|, that icon is returned.
  // Else, if there is a default icon set for the extension action, the icon is
  // created using IconImage. Observer is triggered wheniever the icon gets
  // updated.
  // Else, the extension's placeholder icon is returned.
  // In all cases, action's attention and animation icon transformations are
  // applied on the icon.
  gfx::Image GetIcon(int tab_id);

 private:
  raw_ptr<const ExtensionAction, DanglingUntriaged> action_;
  raw_ptr<Observer, DanglingUntriaged> observer_;
  const bool should_check_icons_;
  gfx::Image cached_default_icon_image_;

  base::ScopedObservation<IconImage,
                          IconImage::Observer>
      icon_image_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_ACTION_ICON_FACTORY_H_
