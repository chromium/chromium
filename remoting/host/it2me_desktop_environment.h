// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_IT2ME_DESKTOP_ENVIRONMENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/basic_desktop_environment.h"

namespace remoting {

class HostWindow;
class LocalInputMonitor;

// Same as BasicDesktopEnvironment but also presents the Continue window to
// the local user.
class It2MeDesktopEnvironment : public BasicDesktopEnvironment {
 public:
  ~It2MeDesktopEnvironment() override;

 protected:
  friend class It2MeDesktopEnvironmentFactory;
  It2MeDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control,
      const DesktopEnvironmentOptions& options);

 private:
  // Presents the continue window to the local user.
  std::unique_ptr<HostWindow> continue_window_;

  // Presents the disconnect window to the local user.
  std::unique_ptr<HostWindow> disconnect_window_;

  // Notifies the client session about the local mouse movements.
  std::unique_ptr<LocalInputMonitor> local_input_monitor_;

  DISALLOW_COPY_AND_ASSIGN(It2MeDesktopEnvironment);
};

// Used to create |It2MeDesktopEnvironment| instances.
class It2MeDesktopEnvironmentFactory : public BasicDesktopEnvironmentFactory {
 public:
  It2MeDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  ~It2MeDesktopEnvironmentFactory() override;

  // DesktopEnvironmentFactory interface.
  std::unique_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control,
      const DesktopEnvironmentOptions& options) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(It2MeDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_DESKTOP_ENVIRONMENT_H_
