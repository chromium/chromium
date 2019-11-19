// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_RESIZING_HOST_OBSERVER_H_
#define REMOTING_HOST_RESIZING_HOST_OBSERVER_H_

#include <stddef.h>

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/host/screen_controls.h"
#include "remoting/host/screen_resolution.h"

namespace remoting {

class DesktopResizer;

// TODO(alexeypa): Rename this class to reflect that it is not
// HostStatusObserver any more.

// Uses the specified DesktopResizer to match host desktop size to the client
// view size as closely as is possible. When the connection closes, restores
// the original desktop size if restore is true.
class ResizingHostObserver : public ScreenControls {
 public:
  explicit ResizingHostObserver(
      std::unique_ptr<DesktopResizer> desktop_resizer,
      bool restore);
  ~ResizingHostObserver() override;

  // ScreenControls interface.
  void SetScreenResolution(const ScreenResolution& resolution) override;

  // Provide a replacement for base::TimeTicks::Now so that this class can be
  // unit-tested in a timely manner. This function will be called exactly
  // once for each call to SetScreenResolution.
  void SetNowFunctionForTesting(
      const base::Callback<base::TimeTicks(void)>& now_function);

 private:
  void RestoreScreenResolution();

  std::unique_ptr<DesktopResizer> desktop_resizer_;
  ScreenResolution original_resolution_;
  bool restore_;

  // State to manage rate-limiting of desktop resizes.
  base::OneShotTimer deferred_resize_timer_;
  base::TimeTicks previous_resize_time_;
  base::Callback<base::TimeTicks(void)> now_function_;

  base::WeakPtrFactory<ResizingHostObserver> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ResizingHostObserver);
};

}  // namespace remoting

#endif  // REMOTING_HOST_RESIZING_HOST_OBSERVER_H_
