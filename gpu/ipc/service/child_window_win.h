// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_CHILD_WINDOW_WIN_H_
#define GPU_IPC_SERVICE_CHILD_WINDOW_WIN_H_

#include "base/memory/weak_ptr.h"
#include "base/task_runner.h"
#include "base/threading/thread.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"

#include <windows.h>

namespace gpu {

// The window DirectComposition renders into needs to be owned by the process
// that's currently doing the rendering. The class creates and owns a window
// which is reparented by the browser to be a child of its window.
class ChildWindowWin {
 public:
  ChildWindowWin(base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
                 HWND parent_window);
  ~ChildWindowWin();

  bool Initialize();
  HWND window() const { return window_; }

  scoped_refptr<base::TaskRunner> GetTaskRunnerForTesting();

 private:
  // The window owner thread.
  std::unique_ptr<base::Thread> thread_;
  // The eventual parent of the window living in the browser process.
  HWND parent_window_;
  HWND window_;
  // The window is initially created with this parent window. We need to keep it
  // around so that we can destroy it at the end.
  HWND initial_parent_window_;
  base::WeakPtr<ImageTransportSurfaceDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ChildWindowWin);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_CHILD_WINDOW_WIN_H_
