// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_DISPLAY_INFO_MONITOR_H_
#define REMOTING_HOST_DESKTOP_DISPLAY_INFO_MONITOR_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/desktop_display_info_loader.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace remoting {

// This class regularly queries the OS for any changes to the multi-monitor
// display configuration, and reports any changes to the registered callbacks.
// This class ensures that the DisplayInfo is fetched on the UI thread, which
// may be different from the calling thread. This is helpful on platforms where
// REMOTING_MULTI_PROCESS == false, allowing this class to be used on the
// network thread. When REMOTING_MULTI_PROCESS == true, this instance lives in
// the Desktop process.
class DesktopDisplayInfoMonitor {
 public:
  using CallbackSignature = void(const DesktopDisplayInfo&);
  using Callback = base::RepeatingCallback<CallbackSignature>;

  explicit DesktopDisplayInfoMonitor(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  DesktopDisplayInfoMonitor(const DesktopDisplayInfoMonitor&) = delete;
  DesktopDisplayInfoMonitor& operator=(const DesktopDisplayInfoMonitor&) =
      delete;

  virtual ~DesktopDisplayInfoMonitor();

  // Begins continuous monitoring for changes. Any changes to the monitor layout
  // will be reported to the registered callbacks.
  void Start();

  // Queries the OS immediately for the current monitor layout and reports any
  // changed display info to the registered callbacks. If this instance is
  // associated with only one DesktopCapturerProxy, this method could be used to
  // query the display info after each captured frame. If there are multiple
  // capturers all linked to this instance, it doesn't make sense to query after
  // every captured frame. So Start() should be called instead, and subsequent
  // calls to QueryDisplayInfo() will have no effect.
  void QueryDisplayInfo();

  // Adds a callback to be notified of display-info changes. Callbacks must not
  // be added after calling Start() or QueryDisplayInfo(). This implementation
  // does not return a base::CallbackListSubscription, so |callback| must either
  // outlive this object, or be bound to a suitable WeakPtr.
  void AddCallback(Callback callback);

 private:
  void QueryDisplayInfoImpl();
  void OnDisplayInfoLoaded(DesktopDisplayInfo info);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // Callbacks which receive DesktopDisplayInfo updates.
  base::RepeatingCallbackList<CallbackSignature> callback_list_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Contains the most recently gathered info about the desktop displays.
  DesktopDisplayInfo desktop_display_info_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Created on the calling thread, but accessed and destroyed on the UI thread.
  std::unique_ptr<DesktopDisplayInfoLoader> desktop_display_info_loader_;

  // Timer to regularly poll |desktop_display_info_loader_| for updates.
  base::RepeatingTimer timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool timer_running_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  base::WeakPtrFactory<DesktopDisplayInfoMonitor> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_DISPLAY_INFO_MONITOR_H_
