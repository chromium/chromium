// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_DISPLAY_INFO_MONITOR_H_
#define REMOTING_HOST_DESKTOP_DISPLAY_INFO_MONITOR_H_

#include <memory>

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

class ClientSessionControl;

// This class regularly queries the OS for any changes to the multi-monitor
// display configuration, and reports any changes to the ClientSession.
// This class ensures that the DisplayInfo is fetched on the UI thread, which
// may be different from the calling thread. This is helpful on platforms where
// REMOTING_MULTI_PROCESS == false, allowing this class to be used on the
// network thread. When REMOTING_MULTI_PROCESS == true, this instance lives in
// the Desktop process.
class DesktopDisplayInfoMonitor {
 public:
  DesktopDisplayInfoMonitor(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control);

  DesktopDisplayInfoMonitor(const DesktopDisplayInfoMonitor&) = delete;
  DesktopDisplayInfoMonitor& operator=(const DesktopDisplayInfoMonitor&) =
      delete;

  virtual ~DesktopDisplayInfoMonitor();

  // Begins continuous monitoring for changes. Any changes to the monitor layout
  // will be reported to the ClientSessionControl.
  void Start();

  // Queries the OS immediately for the current monitor layout and reports any
  // changed display info to the ClientSessionControl. If this instance is
  // associated with a DesktopCapturerProxy, this method could be used to
  // query the display info on each captured frame.
  void QueryDisplayInfo();

 private:
  void OnDisplayInfoLoaded(DesktopDisplayInfo info);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // Object which receives DesktopDisplayInfo updates.
  base::WeakPtr<ClientSessionControl> client_session_control_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Contains the most recently gathered info about the desktop displays.
  DesktopDisplayInfo desktop_display_info_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Created on the calling thread, but accessed and destroyed on the UI thread.
  std::unique_ptr<DesktopDisplayInfoLoader> desktop_display_info_loader_;

  // Timer to regularly poll |desktop_display_info_loader_| for updates.
  base::RepeatingTimer timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<DesktopDisplayInfoMonitor> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_DISPLAY_INFO_MONITOR_H_
