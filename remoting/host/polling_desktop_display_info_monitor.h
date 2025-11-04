// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_POLLING_DESKTOP_DISPLAY_INFO_MONITOR_H_
#define REMOTING_HOST_POLLING_DESKTOP_DISPLAY_INFO_MONITOR_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/desktop_display_info_loader.h"
#include "remoting/host/desktop_display_info_monitor.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace remoting {

// A DesktopDisplayInfoMonitor implementation that monitors display config
// changes by polling the current display config.
// This class ensures that the DisplayInfo is fetched on the UI thread, which
// may be different from the calling thread. This is helpful on platforms where
// REMOTING_MULTI_PROCESS == false, allowing this class to be used on the
// network thread. When REMOTING_MULTI_PROCESS == true, this instance lives in
// the Desktop process.
class PollingDesktopDisplayInfoMonitor : public DesktopDisplayInfoMonitor {
 public:
  explicit PollingDesktopDisplayInfoMonitor(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      std::unique_ptr<DesktopDisplayInfoLoader> info_loader);

  PollingDesktopDisplayInfoMonitor(const PollingDesktopDisplayInfoMonitor&) =
      delete;
  PollingDesktopDisplayInfoMonitor& operator=(
      const PollingDesktopDisplayInfoMonitor&) = delete;

  ~PollingDesktopDisplayInfoMonitor() override;

  // DesktopDisplayInfoMonitor implementation.
  void Start() override;
  bool IsStarted() const override;
  const DesktopDisplayInfo* GetLatestDisplayInfo() const override;
  void AddCallback(base::RepeatingClosure callback) override;

  base::WeakPtr<PollingDesktopDisplayInfoMonitor> GetWeakPtr();

 private:
  void QueryDisplayInfo();
  void OnDisplayInfoLoaded(DesktopDisplayInfo info);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // Callbacks which receive DesktopDisplayInfo updates.
  base::RepeatingClosureList callback_list_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Contains the most recently gathered info about the desktop displays.
  std::optional<DesktopDisplayInfo> desktop_display_info_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Created on the calling thread, but accessed and destroyed on the UI thread.
  std::unique_ptr<DesktopDisplayInfoLoader> desktop_display_info_loader_;

  // Timer to regularly poll |desktop_display_info_loader_| for updates.
  base::RepeatingTimer timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<PollingDesktopDisplayInfoMonitor> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_POLLING_DESKTOP_DISPLAY_INFO_MONITOR_H_
