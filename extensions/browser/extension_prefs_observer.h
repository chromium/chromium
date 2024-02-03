// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_PREFS_OBSERVER_H_
#define EXTENSIONS_BROWSER_EXTENSION_PREFS_OBSERVER_H_

#include "base/time/time.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class ExtensionPrefs;

class ExtensionPrefsObserver {
 public:
  // Called when the reasons for an extension being disabled have changed.
  // This is *not* called when the disable reasons change due to the extension
  // being enabled/disabled.
  virtual void OnExtensionDisableReasonsChanged(const ExtensionId& extension_id,
                                                int disabled_reasons) {}

  // Called when an extension is registered with ExtensionPrefs.
  virtual void OnExtensionRegistered(const ExtensionId& extension_id,
                                     const base::Time& install_time,
                                     bool is_enabled) {}

  // Called when an extension's prefs have been loaded.
  virtual void OnExtensionPrefsLoaded(const ExtensionId& extension_id,
                                      const ExtensionPrefs* prefs) {}

  // Called when an extension's prefs are deleted.
  virtual void OnExtensionPrefsDeleted(const ExtensionId& extension_id) {}

  // Called when an extension's enabled state pref is changed.
  // Note: This does not necessarily correspond to the extension being loaded/
  // unloaded. For that, observe the ExtensionRegistry, and reconcile that the
  // events might not match up.
  virtual void OnExtensionStateChanged(const ExtensionId& extension_id,
                                       bool state) {}

  // Called when an extension's pref has been updated or changed.
  virtual void OnExtensionPrefsUpdated(const ExtensionId& extension_id) {}

  // Called when the runtime permissions for an extension are changed.
  // TODO(devlin): This is a bit out of place here, and may be better suited on
  // a general "extension permissions" observer, if/when we have one. See
  // discussion at
  // https://chromium-review.googlesource.com/c/chromium/src/+/1196107/3/chrome/browser/extensions/runtime_permissions_observer.h#26.
  virtual void OnExtensionRuntimePermissionsChanged(
      const ExtensionId& extension_id) {}

  // Called when an extension's last-launch-time has changed.
  virtual void OnExtensionLastLaunchTimeChanged(
      const ExtensionId& extension_id,
      const base::Time& last_launch_time) {}

  // Called when the ExtensionPrefs object (the thing that this observer
  // observes) will be destroyed. In response, the observer, |this|, should
  // call "prefs->RemoveObserver(this)", whether directly or indirectly (e.g.
  // via ScopedObservation::Reset).
  virtual void OnExtensionPrefsWillBeDestroyed(ExtensionPrefs* prefs) {}
};

// An ExtensionPrefsObserver that's part of the GetEarlyExtensionPrefsObservers
// mechanism, where the ExtensionPrefsObserver needs to connect to an
// ExtensionPrefs, but the ExtensionPrefs doesn't exist yet. This
// OnExtensionPrefsAvailable method lets the connection happen during or
// shortly after the ExtensionPrefs constructor.
class EarlyExtensionPrefsObserver {
 public:
  // Called when "prefs->AddObserver(observer)" should be called, during or
  // shortly after |prefs|' constructor. OnExtensionPrefsAvailable
  // implementations should make that AddObserver call, but are also
  // responsible for making the matching RemoveObserver call at an appropriate
  // time, no later than during the observer's destructor. Otherwise, the
  // observee (the |prefs| object) will follow a dangling pointer whenever the
  // next event occurs.
  //
  // Making that RemoveObserver call at the right time has to be the
  // responsibility of the observer, not the observee, since the observee does
  // not know when the observer is destroyed or is otherwise no longer
  // interested in events.
  //
  // Given that the observer is responsible for calling RemoveObserver, it is
  // cleaner for the observer, not the observee, to also be responsible for
  // calling AddObserver.
  //
  // The recommended technique for ensuring matching AddObserver and
  // RemoveObserver calls is to used a ScopedObservation.
  //
  // Unlike other ExtensionPrefsObserver methods, this method does not have an
  // "const ExtensionId& extension_id" argument. It is not about any one
  // particular extension, it is about the extension preferences as a whole.
  virtual void OnExtensionPrefsAvailable(ExtensionPrefs* prefs) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_PREFS_OBSERVER_H_
