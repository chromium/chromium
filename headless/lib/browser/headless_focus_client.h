// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_FOCUS_CLIENT_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_FOCUS_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window_observer.h"

namespace headless {

class HeadlessFocusClient : public aura::client::FocusClient,
                            public aura::WindowObserver {
 public:
  HeadlessFocusClient();

  HeadlessFocusClient(const HeadlessFocusClient&) = delete;
  HeadlessFocusClient& operator=(const HeadlessFocusClient&) = delete;

  ~HeadlessFocusClient() override;

 private:
  // Overridden from aura::client::FocusClient:
  void AddObserver(aura::client::FocusChangeObserver* observer) override;
  void RemoveObserver(aura::client::FocusChangeObserver* observer) override;
  void FocusWindow(aura::Window* window) override;
  void ResetFocusWithinActiveWindow(aura::Window* window) override;
  aura::Window* GetFocusedWindow() override;

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  raw_ptr<aura::Window> focused_window_;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      observation_manager_{this};
  base::ObserverList<aura::client::FocusChangeObserver> focus_observers_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_FOCUS_CLIENT_H_
