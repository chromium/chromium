// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SCOPED_ACTIVE_INSTALL_H_
#define EXTENSIONS_BROWSER_SCOPED_ACTIVE_INSTALL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "extensions/browser/install_observer.h"
#include "extensions/browser/install_tracker.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

struct ActiveInstallData;

// Registers and deregisters an active extension install with InstallTracker.
class ScopedActiveInstall : public InstallObserver {
 public:
  // This constructor registers an active install with InstallTracker.
  ScopedActiveInstall(InstallTracker* tracker,
                      const ActiveInstallData& install_data);

  // This constructor does not register an active install. The extension install
  // is still deregistered upon destruction.
  ScopedActiveInstall(InstallTracker* tracker, const std::string& extension_id);

  ScopedActiveInstall(const ScopedActiveInstall&) = delete;
  ScopedActiveInstall& operator=(const ScopedActiveInstall&) = delete;

  ~ScopedActiveInstall() override;

  // Ensures that the active install is not deregistered upon destruction. This
  // may be necessary if the extension install outlives the lifetime of this
  // instance.
  void CancelDeregister();

 private:
  void Init();

  // InstallObserver implementation.
  void OnShutdown() override;

  raw_ptr<InstallTracker> tracker_;
  base::ScopedObservation<InstallTracker, InstallObserver> tracker_observation_{
      this};
  const ExtensionId extension_id_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SCOPED_ACTIVE_INSTALL_H_
