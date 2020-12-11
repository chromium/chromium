// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/background_page_watcher.h"

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"

namespace extensions {

BackgroundPageWatcher::BackgroundPageWatcher(ProcessManager* process_manager,
                                             const Extension* extension)
    : process_manager_(process_manager),
      extension_id_(extension->id()),
      is_waiting_for_open_(false),
      is_waiting_for_close_(false) {}

BackgroundPageWatcher::~BackgroundPageWatcher() {}

void BackgroundPageWatcher::WaitForOpen() {
  WaitForOpenState(true);
}

void BackgroundPageWatcher::WaitForClose() {
  WaitForOpenState(false);
}

void BackgroundPageWatcher::WaitForOpenState(bool wait_for_open) {
  if (IsBackgroundPageOpen() == wait_for_open)
    return;
  ScopedObserver<ProcessManager, ProcessManagerObserver> observer(this);
  observer.Add(process_manager_);
  bool* flag = wait_for_open ? &is_waiting_for_open_ : &is_waiting_for_close_;
  base::AutoReset<bool> set_flag(flag, true);
  base::RunLoop run_loop;
  base::AutoReset<base::OnceClosure> set_quit_run_loop(&quit_run_loop_,
                                                       run_loop.QuitClosure());
  run_loop.Run();
  DCHECK_EQ(wait_for_open, IsBackgroundPageOpen());
}

bool BackgroundPageWatcher::IsBackgroundPageOpen() {
  ExtensionHost* host =
      process_manager_->GetBackgroundHostForExtension(extension_id_);
  if (!host)
    return false;
  content::RenderProcessHost* rph =
      host->host_contents()->GetMainFrame()->GetProcess();
  return rph && rph->IsInitializedAndNotDead();
}

void BackgroundPageWatcher::OnExtensionFrameRegistered(
    const std::string& extension_id,
    content::RenderFrameHost* rfh) {
  if (is_waiting_for_open_ && extension_id == extension_id_ &&
      IsBackgroundPageOpen())
    std::move(quit_run_loop_).Run();
}

void BackgroundPageWatcher::OnExtensionFrameUnregistered(
    const std::string& extension_id,
    content::RenderFrameHost* rfh) {
  if (is_waiting_for_close_ && extension_id == extension_id_ &&
      !IsBackgroundPageOpen())
    std::move(quit_run_loop_).Run();
}

}  // namespace extensions
