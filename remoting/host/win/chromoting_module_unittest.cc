// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/chromoting_module.h"

#include <objbase.h>

#include <wrl/client.h>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_local.h"
#include "base/time/time.h"
#include "base/win/atl.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/win/chromoting_lib.h"
#include "remoting/host/win/rdp_desktop_session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

// ChromotingModuleThread runs ChromotingModule::Run() on a dedicated OS thread
// that has no pre-existing TaskEnvironment or SingleThreadTaskExecutor.
class ChromotingModuleThread : public base::SimpleThread {
 public:
  ChromotingModuleThread() : base::SimpleThread("ChromotingModuleTestThread") {}

  ChromotingModuleThread(const ChromotingModuleThread&) = delete;
  ChromotingModuleThread& operator=(const ChromotingModuleThread&) = delete;

  ~ChromotingModuleThread() override {
    if (HasBeenStarted() && !HasBeenJoined()) {
      Join();
    }
    ATL::_pAtlModule = nullptr;
  }

  void Run() override {
    ChromotingModule module;
    success_ = module.Run();
  }

  bool success() const { return success_; }

 private:
  bool success_ = false;
};

// Thread-local storage to hold ComPtr<IRdpDesktopSession> instances on the
// ChromotingModuleThread to prevent COM pointer smuggling between apartments.
struct TlsSessions {
  Microsoft::WRL::ComPtr<IRdpDesktopSession> session1;
  Microsoft::WRL::ComPtr<IRdpDesktopSession> session2;
};

base::ThreadLocalOwnedPointer<TlsSessions>& GetTlsSessionsSlot() {
  static base::NoDestructor<base::ThreadLocalOwnedPointer<TlsSessions>> slot;
  return *slot;
}

TlsSessions& GetOrCreateTlsSessions() {
  if (!GetTlsSessionsSlot().Get()) {
    GetTlsSessionsSlot().Set(std::make_unique<TlsSessions>());
  }
  return *GetTlsSessionsSlot().Get();
}

void ResetTlsSessions() {
  GetTlsSessionsSlot().Set(nullptr);
}

// Waits for ChromotingModule::task_runner() to be initialized.
scoped_refptr<AutoThreadTaskRunner> WaitForModuleTaskRunner() {
  const base::TimeDelta kTimeout = base::Seconds(10);
  const base::TimeTicks deadline = base::TimeTicks::Now() + kTimeout;
  while (base::TimeTicks::Now() < deadline) {
    scoped_refptr<AutoThreadTaskRunner> task_runner =
        ChromotingModule::task_runner();
    if (task_runner) {
      return task_runner;
    }
    base::PlatformThread::Sleep(base::Milliseconds(10));
  }
  return nullptr;
}

// Helper to create an RdpDesktopSession instance managed by ATL.
HRESULT CreateRdpDesktopSession(
    Microsoft::WRL::ComPtr<IRdpDesktopSession>* session) {
  ATL::CComObject<RdpDesktopSession>* raw_session = nullptr;
  HRESULT hr = ATL::CComObject<RdpDesktopSession>::CreateInstance(&raw_session);
  if (FAILED(hr)) {
    return hr;
  }
  *session = raw_session;
  return S_OK;
}

}  // namespace

class ChromotingModuleTest : public testing::Test {
 public:
  ChromotingModuleTest() = default;
  ~ChromotingModuleTest() override = default;

  void SetUp() override {
    ASSERT_FALSE(ATL::_pAtlModule);

    module_thread_.Start();
    task_runner_ = WaitForModuleTaskRunner();
    ASSERT_NE(task_runner_, nullptr);
  }

  void TearDown() override {
    if (module_thread_.HasBeenStarted() && !module_thread_.HasBeenJoined()) {
      WaitForModuleShutdown();
    }
  }

 protected:
  void WaitForModuleShutdown() {
    task_runner_ = nullptr;
    module_thread_.Join();
    EXPECT_TRUE(module_thread_.success());
    EXPECT_EQ(ChromotingModule::task_runner(), nullptr);
  }

  ChromotingModuleThread module_thread_;
  scoped_refptr<AutoThreadTaskRunner> task_runner_;
};

// Tests that ChromotingModule successfully provides an RdpDesktopSession
// instance and terminates when the session is released.
// Note: Per requirements, no methods from RdpDesktopSession are called.
TEST_F(ChromotingModuleTest, ProvideRdpDesktopSessionAndTerminateOnRelease) {
  base::WaitableEvent session_created;
  bool session_created_success = false;

  task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto& sessions = GetOrCreateTlsSessions();
        HRESULT hr = CreateRdpDesktopSession(&sessions.session1);
        EXPECT_HRESULT_SUCCEEDED(hr);
        EXPECT_NE(sessions.session1.Get(), nullptr);
        session_created_success = (sessions.session1 != nullptr);
        session_created.Signal();
      }));

  session_created.Wait();
  ASSERT_TRUE(session_created_success);

  // Release the session on the module's thread/apartment, triggering Unlock().
  base::WaitableEvent session_released;
  task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                           GetOrCreateTlsSessions().session1.Reset();
                           ResetTlsSessions();
                           session_released.Signal();
                         }));

  session_released.Wait();

  WaitForModuleShutdown();
}

// Tests that ChromotingModule remains active while at least one
// RdpDesktopSession is held, and terminates only after all sessions are
// released.
TEST_F(ChromotingModuleTest, TerminateOnlyWhenAllSessionsReleased) {
  base::WaitableEvent sessions_created;
  bool sessions_created_success = false;

  task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto& sessions = GetOrCreateTlsSessions();
        HRESULT hr = CreateRdpDesktopSession(&sessions.session1);
        EXPECT_HRESULT_SUCCEEDED(hr);
        EXPECT_NE(sessions.session1.Get(), nullptr);

        hr = CreateRdpDesktopSession(&sessions.session2);
        EXPECT_HRESULT_SUCCEEDED(hr);
        EXPECT_NE(sessions.session2.Get(), nullptr);

        sessions_created_success =
            (sessions.session1 != nullptr && sessions.session2 != nullptr);
        sessions_created.Signal();
      }));

  sessions_created.Wait();
  ASSERT_TRUE(sessions_created_success);

  // Release session 1 on the module thread.
  base::WaitableEvent session1_released;
  task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                           GetOrCreateTlsSessions().session1.Reset();
                           session1_released.Signal();
                         }));

  session1_released.Wait();

  // Verify that the module is still running because session2 is active.
  base::WaitableEvent ping_event;
  task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                           auto& sessions = GetOrCreateTlsSessions();
                           EXPECT_NE(sessions.session2.Get(), nullptr);
                           EXPECT_NE(ChromotingModule::task_runner(), nullptr);
                           ping_event.Signal();
                         }));

  EXPECT_TRUE(ping_event.TimedWait(base::Seconds(2)));

  // Now release session 2 on the module thread.
  base::WaitableEvent session2_released;
  task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                           GetOrCreateTlsSessions().session2.Reset();
                           ResetTlsSessions();
                           session2_released.Signal();
                         }));

  session2_released.Wait();

  WaitForModuleShutdown();
}

// Tests RdpDesktopSession creation through IClassFactory and aggregation
// rejection.
TEST_F(ChromotingModuleTest, ClassFactoryCreateAndRejectAggregation) {
  base::WaitableEvent factory_tested;
  task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        Microsoft::WRL::ComPtr<IClassFactory> factory;
        HRESULT hr =
            RdpDesktopSession::_ClassFactoryCreatorClass::CreateInstance(
                reinterpret_cast<void*>(
                    RdpDesktopSession::_CreatorClass::CreateInstance),
                IID_PPV_ARGS(&factory));
        EXPECT_HRESULT_SUCCEEDED(hr);
        EXPECT_NE(factory.Get(), nullptr);
        if (!factory) {
          factory_tested.Signal();
          return;
        }

        // RdpDesktopSession rejects aggregation with CLASS_E_NOAGGREGATION.
        Microsoft::WRL::ComPtr<IRdpDesktopSession> aggregated_session;
        hr = factory->CreateInstance(factory.Get(),
                                     IID_PPV_ARGS(&aggregated_session));
        EXPECT_EQ(hr, CLASS_E_NOAGGREGATION);
        EXPECT_EQ(aggregated_session.Get(), nullptr);

        // Successfully create RdpDesktopSession via IClassFactory.
        auto& sessions = GetOrCreateTlsSessions();
        hr = factory->CreateInstance(nullptr, IID_PPV_ARGS(&sessions.session1));
        EXPECT_HRESULT_SUCCEEDED(hr);
        EXPECT_NE(sessions.session1.Get(), nullptr);

        // Release the session and factory to trigger module shutdown.
        sessions.session1.Reset();
        ResetTlsSessions();
        factory.Reset();
        factory_tested.Signal();
      }));

  factory_tested.Wait();

  WaitForModuleShutdown();
}

}  // namespace remoting
