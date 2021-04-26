// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_BACKGROUND_PAGE_WATCHER_H_
#define EXTENSIONS_TEST_BACKGROUND_PAGE_WATCHER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "extensions/browser/process_manager_observer.h"

namespace extensions {
class Extension;
class ProcessManager;

// Observes the background page load state of an extension. Use this in tests
// which rely on a specific open or close state of a background page.
class BackgroundPageWatcher : public ProcessManagerObserver {
 public:
  BackgroundPageWatcher(ProcessManager* process_manager,
                        const Extension* extension);

  ~BackgroundPageWatcher() override;

  // Returns when the background page is open. If the background page is
  // already open, returns immediately.
  void WaitForOpen();

  // Returns when the background page is closed. If the background page is
  // already closed, returns immediately.
  void WaitForClose();

 private:
  // Returns when the background page has open state of |wait_for_open|. If the
  // background page is already in that state, returns immediately.
  void WaitForOpenState(bool wait_for_open);

  bool IsBackgroundPageOpen();

  // ProcessManagerObserver:
  void OnExtensionFrameRegistered(
      const std::string& extension_id,
      content::RenderFrameHost* render_frame_host) override;
  void OnExtensionFrameUnregistered(
      const std::string& extension_id,
      content::RenderFrameHost* render_frame_host) override;

  ProcessManager* process_manager_;
  const std::string extension_id_;
  base::OnceClosure quit_run_loop_;
  bool is_waiting_for_open_;
  bool is_waiting_for_close_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundPageWatcher);
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_BACKGROUND_PAGE_WATCHER_H_
