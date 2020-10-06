// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "remoting/host/resources.h"
#include "remoting/proto/event.pb.h"
#include "remoting/test/it2me_standalone_host.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include <gtk/gtk.h>

#include "base/linux_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11.h"
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

int main(int argc, const char** argv) {
  base::AtExitManager at_exit_manager;
  base::CommandLine::Init(argc, argv);
  remoting::test::It2MeStandaloneHost host;

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Required in order for us to run multiple X11 threads.
  XInitThreads();

  // Create an X11EventSource so the global X11 connection
  // (x11::Connection::Get()) can dispatch X events.
  auto event_source =
      std::make_unique<ui::X11EventSource>(x11::Connection::Get());

  // Required for any calls into GTK functions, such as the Disconnect and
  // Continue windows. Calling with nullptr arguments because we don't have
  // any command line arguments for gtk to consume.
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_init();
#else
  gtk_init(nullptr, nullptr);
#endif

  // Need to prime the host OS version value for linux to prevent IO on the
  // network thread. base::GetLinuxDistro() caches the result.
  base::GetLinuxDistro();
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)
  remoting::LoadResources("");
  host.StartOutputTimer();
  host.Run();
}
