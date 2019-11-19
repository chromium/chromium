// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPAPI_PROXY_TEST_H_
#define PPAPI_PROXY_PPAPI_PROXY_TEST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "ppapi/proxy/resource_message_test_sink.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/test_globals.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class RunLoop;
class SingleThreadTaskRunner;
}

namespace ppapi {
namespace proxy {

class MessageLoopResource;

// Base class for plugin and host test harnesses. Tests will not use this
// directly. Instead, use the PluginProxyTest, HostProxyTest, or TwoWayTest.
class ProxyTestHarnessBase {
 public:
  enum GlobalsConfiguration {
    PER_THREAD_GLOBALS,
    SINGLETON_GLOBALS
  };

  ProxyTestHarnessBase();
  virtual ~ProxyTestHarnessBase();

  PP_Module pp_module() const { return pp_module_; }
  PP_Instance pp_instance() const { return pp_instance_; }
  ResourceMessageTestSink& sink() { return sink_; }

  virtual PpapiGlobals* GetGlobals() = 0;
  // Returns either the plugin or host dispatcher, depending on the test.
  virtual Dispatcher* GetDispatcher() = 0;

  // Set up the harness using an IPC::TestSink to capture messages.
  virtual void SetUpHarness() = 0;

  // Set up the harness using a real IPC channel.
  virtual void SetUpHarnessWithChannel(
      const IPC::ChannelHandle& channel_handle,
      base::SingleThreadTaskRunner* ipc_task_runner,
      base::WaitableEvent* shutdown_event,
      bool is_client) = 0;

  virtual void TearDownHarness() = 0;

  // Implementation of GetInterface for the dispatcher. This will
  // return NULL for all interfaces unless one is registered by calling
  // RegisterTestInterface();
  const void* GetInterface(const char* name);

  // Allows the test to specify an interface implementation for a given
  // interface name. This will be returned when any of the proxy logic
  // requests a local interface.
  void RegisterTestInterface(const char* name, const void* test_interface);

  // Sends a "supports interface" message to the current dispatcher and returns
  // true if it's supported. This is just for the convenience of tests.
  bool SupportsInterface(const char* name);

 private:
  // Destination for IPC messages sent by the test.
  ResourceMessageTestSink sink_;

  // The module and instance ID associated with the plugin dispatcher.
  PP_Module pp_module_;
  PP_Instance pp_instance_;

  // Stores the data for GetInterface/RegisterTestInterface.
  std::map<std::string, const void*> registered_interfaces_;
};

// Test harness for the plugin side of the proxy.
class PluginProxyTestHarness : public ProxyTestHarnessBase {
 public:
  explicit PluginProxyTestHarness(GlobalsConfiguration globals_config);
  virtual ~PluginProxyTestHarness();

  PluginDispatcher* plugin_dispatcher() { return plugin_dispatcher_.get(); }
  PluginResourceTracker& resource_tracker() {
    return *plugin_globals_->plugin_resource_tracker();
  }
  PluginVarTracker& var_tracker() {
    return *plugin_globals_->plugin_var_tracker();
  }

  // ProxyTestHarnessBase implementation.
  virtual PpapiGlobals* GetGlobals();
  virtual Dispatcher* GetDispatcher();
  virtual void SetUpHarness();
  virtual void SetUpHarnessWithChannel(
      const IPC::ChannelHandle& channel_handle,
      base::SingleThreadTaskRunner* ipc_task_runner,
      base::WaitableEvent* shutdown_event,
      bool is_client);
  virtual void TearDownHarness();

  class PluginDelegateMock : public PluginDispatcher::PluginDelegate,
                             public PluginProxyDelegate {
   public:
    PluginDelegateMock() : ipc_task_runner_(NULL), shutdown_event_() {}
    ~PluginDelegateMock() override {}

    void Init(base::SingleThreadTaskRunner* ipc_task_runner,
              base::WaitableEvent* shutdown_event) {
      ipc_task_runner_ = ipc_task_runner;
      shutdown_event_ = shutdown_event;
    }

    void set_browser_sender(IPC::Sender* browser_sender) {
      browser_sender_ = browser_sender;
    }

    // ProxyChannel::Delegate implementation.
    base::SingleThreadTaskRunner* GetIPCTaskRunner() override;
    base::WaitableEvent* GetShutdownEvent() override;
    IPC::PlatformFileForTransit ShareHandleWithRemote(
        base::PlatformFile handle,
        base::ProcessId remote_pid,
        bool should_close_source) override;
    base::UnsafeSharedMemoryRegion ShareUnsafeSharedMemoryRegionWithRemote(
        const base::UnsafeSharedMemoryRegion& region,
        base::ProcessId remote_pid) override;
    base::ReadOnlySharedMemoryRegion ShareReadOnlySharedMemoryRegionWithRemote(
        const base::ReadOnlySharedMemoryRegion& region,
        base::ProcessId remote_pid) override;

    // PluginDispatcher::PluginDelegate implementation.
    std::set<PP_Instance>* GetGloballySeenInstanceIDSet() override;
    uint32_t Register(PluginDispatcher* plugin_dispatcher) override;
    void Unregister(uint32_t plugin_dispatcher_id) override;

    // PluginProxyDelegate implementation.
    IPC::Sender* GetBrowserSender() override;
    std::string GetUILanguage() override;
    void PreCacheFontForFlash(const void* logfontw) override;
    void SetActiveURL(const std::string& url) override;
    PP_Resource CreateBrowserFont(
        Connection connection,
        PP_Instance instance,
        const PP_BrowserFont_Trusted_Description& desc,
        const Preferences& prefs) override;

   private:
    base::SingleThreadTaskRunner* ipc_task_runner_;  // Weak
    base::WaitableEvent* shutdown_event_;  // Weak
    std::set<PP_Instance> instance_id_set_;
    IPC::Sender* browser_sender_;

    DISALLOW_COPY_AND_ASSIGN(PluginDelegateMock);
  };

 private:
  void CreatePluginGlobals(
      const scoped_refptr<base::TaskRunner>& ipc_task_runner);

  GlobalsConfiguration globals_config_;
  std::unique_ptr<PluginGlobals> plugin_globals_;

  std::unique_ptr<PluginDispatcher> plugin_dispatcher_;
  PluginDelegateMock plugin_delegate_mock_;
};

class PluginProxyTest : public PluginProxyTestHarness, public testing::Test {
 public:
  PluginProxyTest();
  virtual ~PluginProxyTest();

  // testing::Test implementation.
  virtual void SetUp();
  virtual void TearDown();
 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// This class provides support for multi-thread testing. A secondary thread is
// created with a Pepper message loop.
// Subclasses need to implement the two SetUpTestOn*Thread() methods to do the
// actual testing work; and call both PostQuitFor*Thread() when testing is
// done.
class PluginProxyMultiThreadTest
    : public PluginProxyTest,
      public base::DelegateSimpleThread::Delegate {
 public:
  PluginProxyMultiThreadTest();
  ~PluginProxyMultiThreadTest() override;

  // Called before the secondary thread is started, but after all the member
  // variables, including |secondary_thread_| and
  // |secondary_thread_message_loop_|, are initialized.
  virtual void SetUpTestOnMainThread() = 0;

  virtual void SetUpTestOnSecondaryThread() = 0;

  // TEST_F() should call this method.
  void RunTest();

  enum ThreadType {
    MAIN_THREAD,
    SECONDARY_THREAD
  };
  void CheckOnThread(ThreadType thread_type);

  // These can be called on any thread.
  void PostQuitForMainThread();
  void PostQuitForSecondaryThread();

 protected:
  scoped_refptr<MessageLoopResource> secondary_thread_message_loop_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

 private:
  // base::DelegateSimpleThread::Delegate implementation.
  void Run() override;

  void QuitNestedLoop();

  static void InternalSetUpTestOnSecondaryThread(void* user_data,
                                                 int32_t result);

  std::unique_ptr<base::DelegateSimpleThread> secondary_thread_;
  std::unique_ptr<base::RunLoop> nested_main_thread_message_loop_;
};

class HostProxyTestHarness : public ProxyTestHarnessBase {
 public:
  explicit HostProxyTestHarness(GlobalsConfiguration globals_config);
  virtual ~HostProxyTestHarness();

  HostDispatcher* host_dispatcher() { return host_dispatcher_.get(); }
  ResourceTracker& resource_tracker() {
    return *host_globals_->GetResourceTracker();
  }
  VarTracker& var_tracker() {
    return *host_globals_->GetVarTracker();
  }

  // ProxyTestBase implementation.
  virtual PpapiGlobals* GetGlobals();
  virtual Dispatcher* GetDispatcher();
  virtual void SetUpHarness();
  virtual void SetUpHarnessWithChannel(
      const IPC::ChannelHandle& channel_handle,
      base::SingleThreadTaskRunner* ipc_task_runner,
      base::WaitableEvent* shutdown_event,
      bool is_client);
  virtual void TearDownHarness();

  class DelegateMock : public ProxyChannel::Delegate {
   public:
    DelegateMock() : ipc_task_runner_(NULL), shutdown_event_(NULL) {}
    ~DelegateMock() override {}

    void Init(base::SingleThreadTaskRunner* ipc_task_runner,
              base::WaitableEvent* shutdown_event) {
      ipc_task_runner_ = ipc_task_runner;
      shutdown_event_ = shutdown_event;
    }

    // ProxyChannel::Delegate implementation.
    base::SingleThreadTaskRunner* GetIPCTaskRunner() override;
    base::WaitableEvent* GetShutdownEvent() override;
    IPC::PlatformFileForTransit ShareHandleWithRemote(
        base::PlatformFile handle,
        base::ProcessId remote_pid,
        bool should_close_source) override;
    base::UnsafeSharedMemoryRegion ShareUnsafeSharedMemoryRegionWithRemote(
        const base::UnsafeSharedMemoryRegion& region,
        base::ProcessId remote_pid) override;
    base::ReadOnlySharedMemoryRegion ShareReadOnlySharedMemoryRegionWithRemote(
        const base::ReadOnlySharedMemoryRegion& region,
        base::ProcessId remote_pid) override;

   private:
    base::SingleThreadTaskRunner* ipc_task_runner_;  // Weak
    base::WaitableEvent* shutdown_event_;  // Weak

    DISALLOW_COPY_AND_ASSIGN(DelegateMock);
  };

 private:
  void CreateHostGlobals();

  GlobalsConfiguration globals_config_;
  std::unique_ptr<ppapi::TestGlobals> host_globals_;
  std::unique_ptr<HostDispatcher> host_dispatcher_;
  // The host side of the real proxy doesn't lock, so this disables locking for
  // the thread the host side of the test runs on.
  std::unique_ptr<ProxyLock::LockingDisablerForTest> disable_locking_;
  DelegateMock delegate_mock_;
};

class HostProxyTest : public HostProxyTestHarness, public testing::Test {
 public:
  HostProxyTest();
  virtual ~HostProxyTest();

  // testing::Test implementation.
  virtual void SetUp();
  virtual void TearDown();
 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Use this base class to test both sides of a proxy.
class TwoWayTest : public testing::Test {
 public:
  enum TwoWayTestMode {
    TEST_PPP_INTERFACE,
    TEST_PPB_INTERFACE
  };
  TwoWayTest(TwoWayTestMode test_mode);
  virtual ~TwoWayTest();

  HostProxyTestHarness& host() { return host_; }
  PluginProxyTestHarness& plugin() { return plugin_; }
  PP_Module pp_module() const { return host_.pp_module(); }
  PP_Instance pp_instance() const { return host_.pp_instance(); }
  TwoWayTestMode test_mode() { return test_mode_; }

  // testing::Test implementation.
  virtual void SetUp();
  virtual void TearDown();

 protected:
  // Post a task to the thread where the remote harness lives. This
  // is typically used to test the state of the var tracker on the plugin
  // thread. This runs the task synchronously for convenience.
  void PostTaskOnRemoteHarness(const base::Closure& task);

 private:
  TwoWayTestMode test_mode_;
  HostProxyTestHarness host_;
  PluginProxyTestHarness plugin_;
  // In order to use sync IPC, we need to have an IO thread.
  base::Thread io_thread_;
  // The plugin side of the proxy runs on its own thread.
  base::Thread plugin_thread_;
  // The message loop for the main (host) thread.
  base::test::SingleThreadTaskEnvironment task_environment_;

  // Aliases for the host and plugin harnesses; if we're testing a PPP
  // interface, remote_harness will point to plugin_, and local_harness
  // will point to host_.  This makes it convenient when we're starting and
  // stopping the harnesses.
  ProxyTestHarnessBase* remote_harness_;
  ProxyTestHarnessBase* local_harness_;

  base::WaitableEvent channel_created_;
  base::WaitableEvent shutdown_event_;
};

// Used during Gtests when you have a PP_Var that you want to EXPECT is equal
// to a certain constant string value:
//
//   EXPECT_VAR_IS_STRING("foo", my_var);
#define EXPECT_VAR_IS_STRING(str, var) { \
  StringVar* sv = StringVar::FromPPVar(var); \
  EXPECT_TRUE(sv); \
  if (sv) \
    EXPECT_EQ(str, sv->value()); \
}

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPAPI_PROXY_TEST_H_
