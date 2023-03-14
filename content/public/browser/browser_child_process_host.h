// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_

#include <memory>
#include <string>

#include "base/environment.h"
#include "base/functional/callback.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/process_type.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"

#if BUILDFLAG(IS_APPLE)
#include "base/process/port_provider_mac.h"
#endif

namespace base {
class CommandLine;
class PersistentMemoryAllocator;
}

namespace content {

class BrowserChildProcessHostDelegate;
class SandboxedProcessLauncherDelegate;
struct ChildProcessData;

// This represents child processes of the browser process, i.e. plugins. They
// will get terminated at browser shutdown.
class CONTENT_EXPORT BrowserChildProcessHost : public IPC::Sender {
 public:
  // Used to create a child process host. The delegate must outlive this object.
  // |process_type| needs to be either an enum value from ProcessType or an
  // embedder-defined value.
  static std::unique_ptr<BrowserChildProcessHost> Create(
      content::ProcessType process_type,
      BrowserChildProcessHostDelegate* delegate,
      ChildProcessHost::IpcMode ipc_mode);

  // Returns the child process host with unique id |child_process_id|, or
  // nullptr if it doesn't exist. |child_process_id| is NOT the process ID, but
  // is the same unique ID as |ChildProcessData::id|.
  static BrowserChildProcessHost* FromID(int child_process_id);

  ~BrowserChildProcessHost() override {}

  // Derived classes call this to launch the child process asynchronously.
  virtual void Launch(
      std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
      std::unique_ptr<base::CommandLine> cmd_line,
      bool terminate_on_shutdown) = 0;

  virtual const ChildProcessData& GetData() = 0;

  // Returns the ChildProcessHost object used by this object.
  virtual ChildProcessHost* GetHost() = 0;

  // Returns the termination info of a child.
  // |known_dead| indicates that the child is already dead. On Linux, this
  // information is necessary to retrieve accurate information. See
  // ChildProcessLauncher::GetChildTerminationInfo() for more details.
  virtual ChildProcessTerminationInfo GetTerminationInfo(bool known_dead) = 0;

  // Take ownership of a "shared" metrics allocator (if one exists).
  virtual std::unique_ptr<base::PersistentMemoryAllocator>
  TakeMetricsAllocator() = 0;

  // Sets the user-visible name of the process.
  virtual void SetName(const std::u16string& name) = 0;

  // Sets the name of the process used for metrics reporting.
  virtual void SetMetricsName(const std::string& metrics_name) = 0;

  // Set the process. BrowserChildProcessHost will do this when the Launch
  // method is used to start the process. However if the owner of this object
  // doesn't call Launch and starts the process in another way, they need to
  // call this method so that the process is associated with this object.
  virtual void SetProcess(base::Process process) = 0;

#if BUILDFLAG(IS_APPLE)
  // Returns a PortProvider used to get the task port for child processes.
  static base::PortProvider* GetPortProvider();
#endif

  // Allows tests to override host interface binding behavior. Any interface
  // binding request which would normally pass through
  // BrowserChildProcessHostImpl::BindHostReceiver() will pass through
  // |callback| first if non-null. |callback| is only called from the IO thread.
  using BindHostReceiverInterceptor =
      base::RepeatingCallback<void(BrowserChildProcessHost* process_host,
                                   mojo::GenericPendingReceiver* receiver)>;
  static void InterceptBindHostReceiverForTesting(
      BindHostReceiverInterceptor callback);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_
