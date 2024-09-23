// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/id_map.h"
#include "base/no_destructor.h"
#include "content/browser/media/desktop_media_window_registry.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace content {

class DesktopMediaWindowRegistryAura final : public DesktopMediaWindowRegistry,
                                             public aura::WindowObserver {
 public:
  static DesktopMediaWindowRegistryAura* GetInstance() {
    static base::NoDestructor<DesktopMediaWindowRegistryAura> instance;
    return instance.get();
  }

  DesktopMediaWindowRegistryAura() = default;

  DesktopMediaWindowRegistryAura(const DesktopMediaWindowRegistryAura&) =
      delete;
  DesktopMediaWindowRegistryAura& operator=(
      const DesktopMediaWindowRegistryAura&) = delete;

  DesktopMediaID::Id RegisterWindow(gfx::NativeWindow window) final {
    base::IDMap<aura::Window*>::const_iterator it(&registered_windows_);
    for (; !it.IsAtEnd(); it.Advance()) {
      if (it.GetCurrentValue() == window)
        return it.GetCurrentKey();
    }

    window->AddObserver(this);
    return registered_windows_.Add(window);
  }

  gfx::NativeWindow GetWindowById(DesktopMediaID::Id id) final {
    return registered_windows_.Lookup(id);
  }

 private:
  ~DesktopMediaWindowRegistryAura() final = default;

  // WindowObserver overrides.
  void OnWindowDestroying(aura::Window* window) final {
    base::IDMap<aura::Window*>::iterator it(&registered_windows_);
    for (; !it.IsAtEnd(); it.Advance()) {
      if (it.GetCurrentValue() == window) {
        registered_windows_.Remove(it.GetCurrentKey());
        return;
      }
    }
    NOTREACHED_IN_MIGRATION();
  }

  base::IDMap<aura::Window*> registered_windows_;
};

// static
DesktopMediaWindowRegistry* DesktopMediaWindowRegistry::GetInstance() {
  return DesktopMediaWindowRegistryAura::GetInstance();
}

}  // namespace content
