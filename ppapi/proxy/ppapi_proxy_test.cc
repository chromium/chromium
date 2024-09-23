// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppapi_proxy_test.h"

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/message_loop/message_pump_type.h"
#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/message_filter.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_proxy_private.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_message_loop_proxy.h"

namespace ppapi {
namespace proxy {

namespace {
// HostDispatcher requires a PPB_Proxy_Private, so we always provide a fallback
// do-nothing implementation.
void PluginCrashed(PP_Module module) {
  NOTREACHED();
}

PP_Instance GetInstanceForResource(PP_Resource resource) {
  // If a test relies on this, we need to implement it.
  NOTREACHED();
}

void SetReserveInstanceIDCallback(PP_Module module,
                                  PP_Bool (*is_seen)(PP_Module, PP_Instance)) {
  // This function gets called in HostDispatcher's constructor.  We simply don't
  // worry about Instance uniqueness in tests, so we can ignore the call.
}

void AddRefModule(PP_Module module) {}
void ReleaseModule(PP_Module module) {}
PP_Bool IsInModuleDestructor(PP_Module module) { return PP_FALSE; }

PPB_Proxy_Private ppb_proxy_private = {
  &PluginCrashed,
  &GetInstanceForResource,
  &SetReserveInstanceIDCallback,
  &AddRefModule,
  &ReleaseModule,
  &IsInModuleDestructor
};

// We allow multiple harnesses at a time to respond to 'GetInterface' calls.
// We assume that only 1 harness's GetInterface function will ever support a
// given interface name. In practice, there will either be only 1 GetInterface
// handler (for PluginProxyTest or HostProxyTest), or there will be only 2
// GetInterface handlers (for TwoWayTest).  In the latter case, one handler is
// for the PluginProxyTestHarness and should only respond for PPP interfaces,
// and the other handler is for the HostProxyTestHarness which should only
// ever respond for PPB interfaces.
base::ObserverList<ProxyTestHarnessBase>::Unchecked get_interface_handlers_;

const void* MockGetInterface(const char* name) {
  for (auto& observer : get_interface_handlers_) {
    const void* interface = observer.GetInterface(name);
    if (interface)
      return interface;
  }
  if (strcmp(name, PPB_PROXY_PRIVATE_INTERFACE) == 0)
    return &ppb_proxy_private;
  return NULL;
}

void SetUpRemoteHarness(ProxyTestHarnessBase* harness,
                        const IPC::ChannelHandle& handle,
                        base::SingleThreadTaskRunner* ipc_task_runner,
                        base::WaitableEvent* shutdown_event,
                        base::WaitableEvent* harness_set_up) {
  harness->SetUpHarnessWithChannel(handle, ipc_task_runner, shutdown_event,
                                   false);
  harness_set_up->Signal();
}

void TearDownRemoteHarness(ProxyTestHarnessBase* harness,
                           base::WaitableEvent* harness_torn_down) {
  harness->TearDownHarness();
  harness_torn_down->Signal();
}

void RunTaskOnRemoteHarness(base::OnceClosure task,
                            base::WaitableEvent* task_complete) {
  std::move(task).Run();
  task_complete->Signal();
}

}  // namespace

// ProxyTestHarnessBase --------------------------------------------------------

ProxyTestHarnessBase::ProxyTestHarnessBase() : pp_module_(0x98765),
                                               pp_instance_(0x12345) {
  get_interface_handlers_.AddObserver(this);
}

ProxyTestHarnessBase::~ProxyTestHarnessBase() {
  get_interface_handlers_.RemoveObserver(this);
}

const void* ProxyTestHarnessBase::GetInterface(const char* name) {
  return registered_interfaces_[name];
}

void ProxyTestHarnessBase::RegisterTestInterface(const char* name,
                                                 const void* test_interface) {
  registered_interfaces_[name] = test_interface;
}

bool ProxyTestHarnessBase::SupportsInterface(const char* name) {
  sink().ClearMessages();

  // IPC doesn't actually write to this when we send a message manually
  // not actually using IPC.
  bool unused_result = false;
  PpapiMsg_SupportsInterface msg(name, &unused_result);
  GetDispatcher()->OnMessageReceived(msg);

  const IPC::Message* reply_msg =
      sink().GetUniqueMessageMatching(IPC_REPLY_ID);
  EXPECT_TRUE(reply_msg);
  if (!reply_msg)
    return false;

  PpapiMsg_SupportsInterface::ReplyParam reply_data;
  EXPECT_TRUE(PpapiMsg_SupportsInterface::ReadReplyParam(
      reply_msg, &reply_data));

  sink().ClearMessages();
  return std::get<0>(reply_data);
}

// PluginProxyTestHarness ------------------------------------------------------

PluginProxyTestHarness::PluginProxyTestHarness(
    GlobalsConfiguration globals_config)
    : globals_config_(globals_config) {
}

PluginProxyTestHarness::~PluginProxyTestHarness() {
}

PpapiGlobals* PluginProxyTestHarness::GetGlobals() {
  return plugin_globals_.get();
}

Dispatcher* PluginProxyTestHarness::GetDispatcher() {
  return plugin_dispatcher_.get();
}

void PluginProxyTestHarness::SetUpHarness() {
  // These must be first since the dispatcher set-up uses them.
  CreatePluginGlobals(nullptr /* ipc_task_runner */);
  // Some of the methods called during set-up check that the lock is held.
  ProxyAutoLock lock;

  resource_tracker().DidCreateInstance(pp_instance());

  plugin_dispatcher_ = std::make_unique<PluginDispatcher>(
      &MockGetInterface, PpapiPermissions(), false);
  plugin_dispatcher_->InitWithTestSink(&sink());
  // The plugin proxy delegate is needed for
  // |PluginProxyDelegate::GetBrowserSender| which is used
  // in |ResourceCreationProxy::GetConnection| to get the channel to the
  // browser. In this case we just use the |plugin_dispatcher_| as the channel
  // for test purposes.
  plugin_delegate_mock_.set_browser_sender(plugin_dispatcher_.get());
  PluginGlobals::Get()->SetPluginProxyDelegate(&plugin_delegate_mock_);
  plugin_dispatcher_->DidCreateInstance(pp_instance());
}

void PluginProxyTestHarness::SetUpHarnessWithChannel(
    const IPC::ChannelHandle& channel_handle,
    base::SingleThreadTaskRunner* ipc_task_runner,
    base::WaitableEvent* shutdown_event,
    bool is_client) {
  // These must be first since the dispatcher set-up uses them.
  CreatePluginGlobals(ipc_task_runner);
  // Some of the methods called during set-up check that the lock is held.
  ProxyAutoLock lock;

  resource_tracker().DidCreateInstance(pp_instance());
  plugin_delegate_mock_.Init(ipc_task_runner, shutdown_event);

  plugin_dispatcher_ = std::make_unique<PluginDispatcher>(
      &MockGetInterface, PpapiPermissions(), false);
  plugin_dispatcher_->InitPluginWithChannel(&plugin_delegate_mock_,
                                            base::kNullProcessId,
                                            channel_handle,
                                            is_client);
  plugin_delegate_mock_.set_browser_sender(plugin_dispatcher_.get());
  PluginGlobals::Get()->SetPluginProxyDelegate(&plugin_delegate_mock_);
  plugin_dispatcher_->DidCreateInstance(pp_instance());
}

void PluginProxyTestHarness::TearDownHarness() {
  {
    // Some of the methods called during tear-down check that the lock is held.
    ProxyAutoLock lock;

    plugin_dispatcher_->DidDestroyInstance(pp_instance());
    plugin_dispatcher_.reset();

    resource_tracker().DidDeleteInstance(pp_instance());
  }
  plugin_globals_.reset();
}

void PluginProxyTestHarness::CreatePluginGlobals(
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner) {
  if (globals_config_ == PER_THREAD_GLOBALS) {
    plugin_globals_ = std::make_unique<PluginGlobals>(
        PpapiGlobals::PerThreadForTest(), ipc_task_runner);
    PpapiGlobals::SetPpapiGlobalsOnThreadForTest(GetGlobals());
  } else {
    plugin_globals_ = std::make_unique<PluginGlobals>(ipc_task_runner);
  }
}

base::SingleThreadTaskRunner*
PluginProxyTestHarness::PluginDelegateMock::GetIPCTaskRunner() {
  return ipc_task_runner_;
}

base::WaitableEvent*
PluginProxyTestHarness::PluginDelegateMock::GetShutdownEvent() {
  return shutdown_event_;
}

IPC::PlatformFileForTransit
PluginProxyTestHarness::PluginDelegateMock::ShareHandleWithRemote(
    base::PlatformFile handle,
    base::ProcessId /* remote_pid */,
    bool should_close_source) {
  return IPC::GetPlatformFileForTransit(handle,
                                        should_close_source);
}

base::UnsafeSharedMemoryRegion PluginProxyTestHarness::PluginDelegateMock::
    ShareUnsafeSharedMemoryRegionWithRemote(
        const base::UnsafeSharedMemoryRegion& region,
        base::ProcessId /* remote_pid */) {
  return region.Duplicate();
}

base::ReadOnlySharedMemoryRegion PluginProxyTestHarness::PluginDelegateMock::
    ShareReadOnlySharedMemoryRegionWithRemote(
        const base::ReadOnlySharedMemoryRegion& region,
        base::ProcessId /* remote_pid */) {
  return region.Duplicate();
}

std::set<PP_Instance>*
PluginProxyTestHarness::PluginDelegateMock::GetGloballySeenInstanceIDSet() {
  return &instance_id_set_;
}

uint32_t PluginProxyTestHarness::PluginDelegateMock::Register(
    PluginDispatcher* plugin_dispatcher) {
  return 0;
}

void PluginProxyTestHarness::PluginDelegateMock::Unregister(
    uint32_t plugin_dispatcher_id) {}

IPC::Sender* PluginProxyTestHarness::PluginDelegateMock::GetBrowserSender() {
  return browser_sender_;
}

std::string PluginProxyTestHarness::PluginDelegateMock::GetUILanguage() {
  return std::string("en-US");
}

void PluginProxyTestHarness::PluginDelegateMock::SetActiveURL(
    const std::string& url) {
}

PP_Resource PluginProxyTestHarness::PluginDelegateMock::CreateBrowserFont(
    Connection connection,
    PP_Instance instance,
    const PP_BrowserFont_Trusted_Description& desc,
    const Preferences& prefs) {
  return 0;
}

// PluginProxyTest -------------------------------------------------------------

PluginProxyTest::PluginProxyTest() : PluginProxyTestHarness(SINGLETON_GLOBALS) {
}

PluginProxyTest::~PluginProxyTest() {
}

void PluginProxyTest::SetUp() {
  SetUpHarness();
}

void PluginProxyTest::TearDown() {
  TearDownHarness();
}

// PluginProxyMultiThreadTest --------------------------------------------------

PluginProxyMultiThreadTest::PluginProxyMultiThreadTest() {
}

PluginProxyMultiThreadTest::~PluginProxyMultiThreadTest() {
}

void PluginProxyMultiThreadTest::RunTest() {
  main_thread_task_runner_ = PpapiGlobals::Get()->GetMainThreadMessageLoop();
  ASSERT_EQ(main_thread_task_runner_.get(),
            base::SingleThreadTaskRunner::GetCurrentDefault().get());
  nested_main_thread_message_loop_ = std::make_unique<base::RunLoop>();

  secondary_thread_ = std::make_unique<base::DelegateSimpleThread>(
      this, "PluginProxyMultiThreadTest");

  {
    ProxyAutoLock auto_lock;

    // MessageLoopResource assumes that the proxy lock has been acquired.
    secondary_thread_message_loop_ = new MessageLoopResource(pp_instance());

    ASSERT_EQ(PP_OK,
        secondary_thread_message_loop_->PostWork(
            PP_MakeCompletionCallback(
                &PluginProxyMultiThreadTest::InternalSetUpTestOnSecondaryThread,
                this),
            0));
  }

  SetUpTestOnMainThread();

  secondary_thread_->Start();
  nested_main_thread_message_loop_->Run();
  secondary_thread_->Join();

  {
    ProxyAutoLock auto_lock;

    // The destruction requires a valid PpapiGlobals instance, so we should
    // explicitly release it.
    secondary_thread_message_loop_.reset();
  }

  secondary_thread_.reset(NULL);
  nested_main_thread_message_loop_.reset(NULL);
  main_thread_task_runner_.reset();
}

void PluginProxyMultiThreadTest::CheckOnThread(ThreadType thread_type) {
  ProxyAutoLock auto_lock;
  if (thread_type == MAIN_THREAD) {
    ASSERT_TRUE(MessageLoopResource::GetCurrent()->is_main_thread_loop());
  } else {
    ASSERT_EQ(secondary_thread_message_loop_.get(),
              MessageLoopResource::GetCurrent());
  }
}

void PluginProxyMultiThreadTest::PostQuitForMainThread() {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PluginProxyMultiThreadTest::QuitNestedLoop,
                                base::Unretained(this)));
}

void PluginProxyMultiThreadTest::PostQuitForSecondaryThread() {
  ProxyAutoLock auto_lock;
  secondary_thread_message_loop_->PostQuit(PP_TRUE);
}

void PluginProxyMultiThreadTest::Run() {
  ProxyAutoLock auto_lock;
  ASSERT_EQ(PP_OK, secondary_thread_message_loop_->AttachToCurrentThread());
  ASSERT_EQ(PP_OK, secondary_thread_message_loop_->Run());
  secondary_thread_message_loop_->DetachFromThread();
}

void PluginProxyMultiThreadTest::QuitNestedLoop() {
  nested_main_thread_message_loop_->Quit();
}

// static
void PluginProxyMultiThreadTest::InternalSetUpTestOnSecondaryThread(
    void* user_data,
    int32_t result) {
  EXPECT_EQ(PP_OK, result);
  PluginProxyMultiThreadTest* thiz =
      static_cast<PluginProxyMultiThreadTest*>(user_data);
  thiz->CheckOnThread(SECONDARY_THREAD);
  thiz->SetUpTestOnSecondaryThread();
}

// HostProxyTestHarness --------------------------------------------------------

HostProxyTestHarness::HostProxyTestHarness(GlobalsConfiguration globals_config)
    : globals_config_(globals_config) {
}

HostProxyTestHarness::~HostProxyTestHarness() {
}

PpapiGlobals* HostProxyTestHarness::GetGlobals() {
  return host_globals_.get();
}

Dispatcher* HostProxyTestHarness::GetDispatcher() {
  return host_dispatcher_.get();
}

void HostProxyTestHarness::SetUpHarness() {
  // These must be first since the dispatcher set-up uses them.
  CreateHostGlobals();

  host_dispatcher_ = std::make_unique<HostDispatcher>(
      pp_module(), &MockGetInterface, PpapiPermissions::AllPermissions());
  host_dispatcher_->InitWithTestSink(&sink());
  HostDispatcher::SetForInstance(pp_instance(), host_dispatcher_.get());
}

void HostProxyTestHarness::SetUpHarnessWithChannel(
    const IPC::ChannelHandle& channel_handle,
    base::SingleThreadTaskRunner* ipc_task_runner,
    base::WaitableEvent* shutdown_event,
    bool is_client) {
  // These must be first since the dispatcher set-up uses them.
  CreateHostGlobals();

  delegate_mock_.Init(ipc_task_runner, shutdown_event);

  host_dispatcher_ = std::make_unique<HostDispatcher>(
      pp_module(), &MockGetInterface, PpapiPermissions::AllPermissions());
  ppapi::Preferences preferences;
  host_dispatcher_->InitHostWithChannel(
      &delegate_mock_, base::kNullProcessId, channel_handle, is_client,
      preferences, base::SingleThreadTaskRunner::GetCurrentDefault());
  HostDispatcher::SetForInstance(pp_instance(), host_dispatcher_.get());
}

void HostProxyTestHarness::TearDownHarness() {
  HostDispatcher::RemoveForInstance(pp_instance());
  host_dispatcher_.reset();
  host_globals_.reset();
}

void HostProxyTestHarness::CreateHostGlobals() {
  disable_locking_ = std::make_unique<ProxyLock::LockingDisablerForTest>();
  if (globals_config_ == PER_THREAD_GLOBALS) {
    host_globals_ =
        std::make_unique<TestGlobals>(PpapiGlobals::PerThreadForTest());
    PpapiGlobals::SetPpapiGlobalsOnThreadForTest(GetGlobals());
  } else {
    host_globals_ = std::make_unique<TestGlobals>();
  }
}

base::SingleThreadTaskRunner*
HostProxyTestHarness::DelegateMock::GetIPCTaskRunner() {
  return ipc_task_runner_;
}

base::WaitableEvent* HostProxyTestHarness::DelegateMock::GetShutdownEvent() {
  return shutdown_event_;
}

IPC::PlatformFileForTransit
HostProxyTestHarness::DelegateMock::ShareHandleWithRemote(
    base::PlatformFile handle,
    base::ProcessId /* remote_pid */,
    bool should_close_source) {
  return IPC::GetPlatformFileForTransit(handle,
                                        should_close_source);
}

base::UnsafeSharedMemoryRegion
HostProxyTestHarness::DelegateMock::ShareUnsafeSharedMemoryRegionWithRemote(
    const base::UnsafeSharedMemoryRegion& region,
    base::ProcessId /*remote_pid*/) {
  return region.Duplicate();
}

base::ReadOnlySharedMemoryRegion
HostProxyTestHarness::DelegateMock::ShareReadOnlySharedMemoryRegionWithRemote(
    const base::ReadOnlySharedMemoryRegion& region,
    base::ProcessId /*remote_pid*/) {
  return region.Duplicate();
}

// HostProxyTest ---------------------------------------------------------------

HostProxyTest::HostProxyTest() : HostProxyTestHarness(SINGLETON_GLOBALS) {
}

HostProxyTest::~HostProxyTest() {
}

void HostProxyTest::SetUp() {
  SetUpHarness();
}

void HostProxyTest::TearDown() {
  TearDownHarness();
}

// TwoWayTest ---------------------------------------------------------------

TwoWayTest::TwoWayTest(TwoWayTest::TwoWayTestMode test_mode)
    : test_mode_(test_mode),
      host_(ProxyTestHarnessBase::PER_THREAD_GLOBALS),
      plugin_(ProxyTestHarnessBase::PER_THREAD_GLOBALS),
      io_thread_("TwoWayTest_IOThread"),
      plugin_thread_("TwoWayTest_PluginThread"),
      remote_harness_(NULL),
      local_harness_(NULL),
      channel_created_(base::WaitableEvent::ResetPolicy::MANUAL,
                       base::WaitableEvent::InitialState::NOT_SIGNALED),
      shutdown_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED) {
  if (test_mode == TEST_PPP_INTERFACE) {
    remote_harness_ = &plugin_;
    local_harness_ = &host_;
  } else {
    remote_harness_ = &host_;
    local_harness_ = &plugin_;
  }
}

TwoWayTest::~TwoWayTest() {
  shutdown_event_.Signal();
}

void TwoWayTest::SetUp() {
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  io_thread_.StartWithOptions(std::move(options));
  plugin_thread_.Start();

  mojo::MessagePipe pipe;
  base::WaitableEvent remote_harness_set_up(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  plugin_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&SetUpRemoteHarness, remote_harness_,
                                pipe.handle0.release(),
                                base::RetainedRef(io_thread_.task_runner()),
                                &shutdown_event_, &remote_harness_set_up));
  remote_harness_set_up.Wait();
  local_harness_->SetUpHarnessWithChannel(
      pipe.handle1.release(), io_thread_.task_runner().get(), &shutdown_event_,
      true);  // is_client
}

void TwoWayTest::TearDown() {
  base::WaitableEvent remote_harness_torn_down(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  plugin_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TearDownRemoteHarness, remote_harness_,
                                &remote_harness_torn_down));
  remote_harness_torn_down.Wait();

  local_harness_->TearDownHarness();

  io_thread_.Stop();
}

void TwoWayTest::PostTaskOnRemoteHarness(base::OnceClosure task) {
  base::WaitableEvent task_complete(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  plugin_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RunTaskOnRemoteHarness, std::move(task), &task_complete));
  task_complete.Wait();
}

}  // namespace proxy
}  // namespace ppapi
