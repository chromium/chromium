// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_THREAD_IMPL_H_
#define CONTENT_CHILD_CHILD_THREAD_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/power_monitor/power_monitor.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/variations/child_process_field_trial_syncer.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/child_control.mojom.h"
#include "content/common/content_export.h"
#include "content/public/child/child_thread.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_buildflags.h"  // For BUILDFLAG(IPC_MESSAGE_LOG_ENABLED).
#include "ipc/ipc_platform_file.h"
#include "ipc/message_router.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "third_party/blink/public/mojom/associated_interfaces/associated_interfaces.mojom.h"

#if defined(OS_WIN)
#include "content/public/common/font_cache_win.mojom.h"
#elif defined(OS_MACOSX)
#include "content/common/font_loader_mac.mojom.h"
#endif

namespace IPC {
class MessageFilter;
class SyncChannel;
class SyncMessageFilter;
}  // namespace IPC

namespace mojo {
class OutgoingInvitation;
namespace core {
class ScopedIPCSupport;
}  // namespace core
}  // namespace mojo

namespace content {
class InProcessChildThreadParams;
class ThreadSafeSender;

// The main thread of a child process derives from this class.
class CONTENT_EXPORT ChildThreadImpl
    : public IPC::Listener,
      virtual public ChildThread,
      private base::FieldTrialList::Observer,
      public mojom::RouteProvider,
      public blink::mojom::AssociatedInterfaceProvider,
      public mojom::ChildControl {
 public:
  struct CONTENT_EXPORT Options;

  // Creates the thread.
  explicit ChildThreadImpl(base::RepeatingClosure quit_closure);
  // Allow to be used for single-process mode and for in process gpu mode via
  // options.
  ChildThreadImpl(base::RepeatingClosure quit_closure, const Options& options);
  // ChildProcess::main_thread() is reset after Shutdown(), and before the
  // destructor, so any subsystem that relies on ChildProcess::main_thread()
  // must be terminated before Shutdown returns. In particular, if a subsystem
  // has a thread that post tasks to ChildProcess::main_thread(), that thread
  // should be joined in Shutdown().
  ~ChildThreadImpl() override;
  virtual void Shutdown();
  // Returns true if the thread should be destroyed.
  virtual bool ShouldBeDestroyed();

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  // ChildThread implementation:
#if defined(OS_WIN)
  void PreCacheFont(const LOGFONT& log_font) override;
  void ReleaseCachedFonts() override;
#elif defined(OS_MACOSX)
  bool LoadFont(const base::string16& font_name,
                float font_point_size,
                mojo::ScopedSharedBufferHandle* out_font_data,
                uint32_t* out_font_id) override;
#endif
  void RecordAction(const base::UserMetricsAction& action) override;
  void RecordComputedAction(const std::string& action) override;
  ServiceManagerConnection* GetServiceManagerConnection() override;
  service_manager::Connector* GetConnector() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() override;
  void SetFieldTrialGroup(const std::string& trial_name,
                          const std::string& group_name) override;

  // base::FieldTrialList::Observer:
  void OnFieldTrialGroupFinalized(const std::string& trial_name,
                                  const std::string& group_name) override;

  IPC::SyncChannel* channel() { return channel_.get(); }

  IPC::MessageRouter* GetRouter();

  mojom::RouteProvider* GetRemoteRouteProvider();

  // Allocates a block of shared memory of the given size. Returns nullptr on
  // failure.
  static std::unique_ptr<base::SharedMemory> AllocateSharedMemory(
      size_t buf_size);

  IPC::SyncMessageFilter* sync_message_filter() const {
    return sync_message_filter_.get();
  }

  // The getter should only be called on the main thread, however the
  // IPC::Sender it returns may be safely called on any thread including
  // the main thread.
  ThreadSafeSender* thread_safe_sender() const {
    return thread_safe_sender_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner() const {
    return main_thread_runner_;
  }

  // Returns the one child thread. Can only be called on the main thread.
  static ChildThreadImpl* current();

#if defined(OS_ANDROID)
  // Called on Android's service thread to shutdown the main thread of this
  // process.
  static void ShutdownThread();
#endif

 protected:
  friend class ChildProcess;

  // Called when the process refcount is 0.
  virtual void OnProcessFinalRelease();

  // Called by subclasses to manually start the ServiceManagerConnection. Must
  // only be called if
  // ChildThreadImpl::Options::auto_start_service_manager_connection was set to
  // |false| on ChildThreadImpl construction.
  void StartServiceManagerConnection();

  // mojom::ChildControl
  void ProcessShutdown() override;
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  void SetIPCLoggingEnabled(bool enable) override;
#endif
  void OnChildControlRequest(mojom::ChildControlRequest);

  virtual bool OnControlMessageReceived(const IPC::Message& msg);
  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;
  bool on_channel_error_called() const { return on_channel_error_called_; }

  bool IsInBrowserProcess() const;

#if defined(OS_MACOSX)
  virtual mojom::FontLoaderMac* GetFontLoaderMac();
#endif

 private:
  class ChildThreadMessageRouter : public IPC::MessageRouter {
   public:
    // |sender| must outlive this object.
    explicit ChildThreadMessageRouter(IPC::Sender* sender);
    bool Send(IPC::Message* msg) override;

    // MessageRouter overrides.
    bool RouteMessage(const IPC::Message& msg) override;

   private:
    IPC::Sender* const sender_;
  };

  void Init(const Options& options);

  // Sets chrome_trace_event_agent_ if necessary.
  void InitTracing();

  // We create the channel first without connecting it so we can add filters
  // prior to any messages being received, then connect it afterwards.
  void ConnectChannel();

  // IPC message handlers.

  void EnsureConnected();

  // mojom::RouteProvider:
  void GetRoute(int32_t routing_id,
                blink::mojom::AssociatedInterfaceProviderAssociatedRequest
                    request) override;

  // blink::mojom::AssociatedInterfaceProvider:
  void GetAssociatedInterface(
      const std::string& name,
      blink::mojom::AssociatedInterfaceAssociatedRequest request) override;

#if defined(OS_WIN)
  mojom::FontCacheWin* GetFontCacheWin();
#endif

  std::unique_ptr<mojo::core::ScopedIPCSupport> mojo_ipc_support_;
  std::unique_ptr<ServiceManagerConnection> service_manager_connection_;

  mojo::BindingSet<mojom::ChildControl> child_control_bindings_;
  mojo::AssociatedBinding<mojom::RouteProvider> route_provider_binding_;
  mojo::AssociatedBindingSet<blink::mojom::AssociatedInterfaceProvider, int32_t>
      associated_interface_provider_bindings_;
  mojom::RouteProviderAssociatedPtr remote_route_provider_;
#if defined(OS_WIN)
  mojom::FontCacheWinPtr font_cache_win_ptr_;
#elif defined(OS_MACOSX)
  mojom::FontLoaderMacPtr font_loader_mac_ptr_;
#endif

  std::unique_ptr<IPC::SyncChannel> channel_;

  // Allows threads other than the main thread to send sync messages.
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  // Implements message routing functionality to the consumers of
  // ChildThreadImpl.
  ChildThreadMessageRouter router_;

  // The OnChannelError() callback was invoked - the channel is dead, don't
  // attempt to communicate.
  bool on_channel_error_called_;

  // TaskRunner to post tasks to the main thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner_;

  // Used to quit the main thread.
  base::RepeatingClosure quit_closure_;

  std::unique_ptr<base::PowerMonitor> power_monitor_;

  scoped_refptr<base::SingleThreadTaskRunner> browser_process_io_runner_;

  std::unique_ptr<tracing::TraceEventAgent> trace_event_agent_;

  std::unique_ptr<variations::ChildProcessFieldTrialSyncer> field_trial_syncer_;

  std::unique_ptr<base::WeakPtrFactory<ChildThreadImpl>>
      channel_connected_factory_;

  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

  base::WeakPtrFactory<ChildThreadImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildThreadImpl);
};

struct ChildThreadImpl::Options {
  Options(const Options& other);
  ~Options();

  class Builder;

  bool auto_start_service_manager_connection;
  bool connect_to_browser;
  scoped_refptr<base::SingleThreadTaskRunner> browser_process_io_runner;
  std::vector<IPC::MessageFilter*> startup_filters;
  mojo::OutgoingInvitation* mojo_invitation;
  std::string in_process_service_request_token;
  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner;

 private:
  Options();
};

class ChildThreadImpl::Options::Builder {
 public:
  Builder();

  Builder& InBrowserProcess(const InProcessChildThreadParams& params);
  Builder& AutoStartServiceManagerConnection(bool auto_start);
  Builder& ConnectToBrowser(bool connect_to_browser);
  Builder& AddStartupFilter(IPC::MessageFilter* filter);
  Builder& IPCTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner);

  Options Build();

 private:
  struct Options options_;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_THREAD_IMPL_H_
