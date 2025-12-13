// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DELEGATING_DESKTOP_DISPLAY_INFO_MONITOR_H_
#define REMOTING_HOST_DELEGATING_DESKTOP_DISPLAY_INFO_MONITOR_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/host/desktop_display_info_monitor.h"

namespace remoting {

// A DesktopDisplayInfoMonitor that delegates calls to the underlying monitor.
// This is useful for sharing the same DesktopDisplayInfoMonitor, which may be
// expensive. Note that each DelegatingDesktopDisplayInfoMonitor will have its
// own state, such that callbacks added before Start() will not be called until
// Start() is called, and the latest known display info will be report after
// Start() is called, independent of the started state of the underlying
// monitor.
class DelegatingDesktopDisplayInfoMonitor : public DesktopDisplayInfoMonitor {
 public:
  explicit DelegatingDesktopDisplayInfoMonitor(
      base::WeakPtr<DesktopDisplayInfoMonitor> underlying);
  ~DelegatingDesktopDisplayInfoMonitor() override;

  // DesktopDisplayInfoMonitor implementation.
  void Start() override;
  bool IsStarted() const override;
  const DesktopDisplayInfo* GetLatestDisplayInfo() const override;
  void AddCallback(base::RepeatingClosure callback) override;

 private:
  void OnUnderlyingDisplayInfoChanged();

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<DesktopDisplayInfoMonitor> underlying_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool started_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  base::RepeatingClosureList callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtrFactory<DelegatingDesktopDisplayInfoMonitor> weak_ptr_factory_{
      this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_DELEGATING_DESKTOP_DISPLAY_INFO_MONITOR_H_
