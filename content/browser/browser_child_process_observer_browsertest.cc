// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_child_process_observer.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/child_process_host_impl.h"
#include "content/browser/utility_process_host.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_service.mojom.h"
#include "sandbox/policy/sandbox_type.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

namespace {

// An enum that represent the different type of notitifcations that exist in
// BrowserChildProcessObserver.
enum class Notification {
  kLaunchedAndConnected,
  kDisconnected,
  kCrashed,
  kKilled,
  kLaunchFailed,
  kExitedNormally,
};

// Nicer test output.
std::ostream& operator<<(std::ostream& os, Notification notification) {
  switch (notification) {
    case Notification::kLaunchedAndConnected:
      os << "LaunchedAndConnected";
      break;
    case Notification::kDisconnected:
      os << "Disconnected";
      break;
    case Notification::kCrashed:
      os << "Crashed";
      break;
    case Notification::kKilled:
      os << "Killed";
      break;
    case Notification::kLaunchFailed:
      os << "LaunchFailed";
      break;
    case Notification::kExitedNormally:
      os << "ExitedNormally";
      break;
  }
  return os;
}

// Returns true if a child process whose ID is |child_id| is still alive.
bool IsHostAlive(int child_id) {
  return BrowserChildProcessHost::FromID(child_id) != nullptr;
}

}  // namespace

// A test BrowserChildProcessObserver that transforms every call to one of the
// observer's method to a call to the notification callback.
class BrowserChildProcessNotificationObserver
    : public BrowserChildProcessObserver {
 public:
  using OnNotificationCallback =
      base::RepeatingCallback<void(Notification notification)>;

  BrowserChildProcessNotificationObserver(
      int child_id,
      OnNotificationCallback on_notification_callback)
      : child_id_(child_id),
        on_notification_callback_(std::move(on_notification_callback)) {
    BrowserChildProcessObserver::Add(this);
  }

  ~BrowserChildProcessNotificationObserver() override {
    BrowserChildProcessObserver::Remove(this);
  }

 protected:
  // BrowserChildProcessObserver:
  void BrowserChildProcessLaunchedAndConnected(
      const ChildProcessData& data) override {
    OnNotification(data, Notification::kLaunchedAndConnected);
  }
  void BrowserChildProcessHostDisconnected(
      const ChildProcessData& data) override {
    OnNotification(data, Notification::kDisconnected);
  }
  void BrowserChildProcessCrashed(
      const ChildProcessData& data,
      const ChildProcessTerminationInfo& info) override {
    OnNotification(data, Notification::kCrashed);
  }
  void BrowserChildProcessKilled(
      const ChildProcessData& data,
      const ChildProcessTerminationInfo& info) override {
    OnNotification(data, Notification::kKilled);
  }
  void BrowserChildProcessLaunchFailed(
      const ChildProcessData& data,
      const ChildProcessTerminationInfo& info) override {
    OnNotification(data, Notification::kLaunchFailed);
  }
  void BrowserChildProcessExitedNormally(
      const ChildProcessData& data,
      const ChildProcessTerminationInfo& info) override {
    OnNotification(data, Notification::kExitedNormally);
  }

  void OnNotification(const ChildProcessData& data, Notification notification) {
    if (data.id == child_id_)
      on_notification_callback_.Run(notification);
  }

 private:
  // Every notification coming for a child with a different ID will be ignored.
  int child_id_;

  // The callback to invoke every time a method of the observer is called.
  OnNotificationCallback on_notification_callback_;
};

// A helper class that allows the user to wait until a specific |notification|
// is sent for a child process whose ID matches |child_id|.
class WaitForNotificationObserver {
 public:
  WaitForNotificationObserver(int child_id, Notification notification)
      : inner_observer_(
            child_id,
            base::BindRepeating(&WaitForNotificationObserver::OnNotification,
                                base::Unretained(this))),
        notification_(notification) {}

  ~WaitForNotificationObserver() = default;

  // Waits until the notification is received. Returns immediately if it was
  // already received.
  void Wait() {
    if (notification_received_)
      return;

    DCHECK(!run_loop_.running());
    run_loop_.Run();
  }

 private:
  void OnNotification(Notification notification) {
    if (notification != notification_)
      return;

    notification_received_ = true;
    if (run_loop_.running())
      run_loop_.Quit();
  }

  BrowserChildProcessNotificationObserver inner_observer_;
  Notification notification_;
  base::RunLoop run_loop_;
  bool notification_received_ = false;
};

class TestSandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  explicit TestSandboxedProcessLauncherDelegate(
      sandbox::mojom::Sandbox sandbox_type)
      : sandbox_type_(sandbox_type) {}
  ~TestSandboxedProcessLauncherDelegate() override = default;

  // SandboxedProcessLauncherDelegate:
  sandbox::mojom::Sandbox GetSandboxType() override { return sandbox_type_; }

 private:
  sandbox::mojom::Sandbox sandbox_type_;
};

// A test-specific type of process host. Self-owned.
class TestProcessHost : public BrowserChildProcessHostDelegate {
 public:
  static base::WeakPtr<TestProcessHost> Create() {
    auto* instance = new TestProcessHost();
    return instance->GetWeakPtr();
  }

  TestProcessHost()
      : process_(BrowserChildProcessHost::Create(
            PROCESS_TYPE_UTILITY,
            this,
            ChildProcessHost::IpcMode::kNormal)) {}
  ~TestProcessHost() override = default;

  // Returns the ID of the child process.
  int GetId() { return process_->GetData().id; }

  // Binds to the test service on the child process and returns the bound
  // remote.
  mojo::Remote<mojom::TestService> BindTestService() {
    mojo::Remote<mojom::TestService> test_service;

    static_cast<ChildProcessHostImpl*>(process_->GetHost())
        ->child_process()
        ->BindServiceInterface(test_service.BindNewPipeAndPassReceiver());

    return test_service;
  }

  // Returns the command line used to launch the child process.
  std::unique_ptr<base::CommandLine> GetChildCommandLine() {
    base::FilePath child_path =
        ChildProcessHost::GetChildPath(ChildProcessHost::CHILD_NORMAL);
    auto command_line = std::make_unique<base::CommandLine>(child_path);

    command_line->AppendSwitchASCII(switches::kProcessType,
                                    switches::kUtilityProcess);
    command_line->AppendSwitchASCII(switches::kUtilitySubType,
                                    "Test Utility Process");
    sandbox::policy::SetCommandLineFlagsForSandboxType(command_line.get(),
                                                       sandbox_type_);

    return command_line;
  }

  // Launches the child process using the default test launcher delegate.
  void LaunchProcess() {
    LaunchProcessWithDelegate(
        std::make_unique<TestSandboxedProcessLauncherDelegate>(sandbox_type_));
  }

  // Launches the child process using a supplied sandbox delegate.
  void LaunchProcessWithDelegate(
      std::unique_ptr<SandboxedProcessLauncherDelegate>
          sandboxed_process_launcher_delegate) {
    process_->SetName(u"Test utility process");

    auto command_line = GetChildCommandLine();
    bool terminate_on_shutdown = true;

    process_->Launch(std::move(sandboxed_process_launcher_delegate),
                     std::move(command_line), terminate_on_shutdown);

    test_service_ = BindTestService();
  }

  // Requests the child process to shutdown.
  void ForceShutdown() { process_->GetHost()->ForceShutdown(); }

  // Disconnects the bound remote from the test service.
  void Disconnect() { test_service_.reset(); }

  // Sets the sandbox type to use for the child process.
  void SetSandboxType(sandbox::mojom::Sandbox sandbox_type) {
    sandbox_type_ = sandbox_type;
  }

  mojom::TestService* service() const { return test_service_.get(); }

  base::WeakPtr<TestProcessHost> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  sandbox::mojom::Sandbox sandbox_type_ = sandbox::mojom::Sandbox::kUtility;

  std::unique_ptr<BrowserChildProcessHost> process_;

  mojo::Remote<mojom::TestService> test_service_;

  base::WeakPtrFactory<TestProcessHost> weak_ptr_factory_{this};
};

// A helper class that exposes which notifications were sent for a specific
// child process.
class TestBrowserChildProcessObserver {
 public:
  explicit TestBrowserChildProcessObserver(int child_id)
      : inner_observer_(child_id,
                        base::BindRepeating(
                            &TestBrowserChildProcessObserver::OnNotification,
                            base::Unretained(this))) {}

  ~TestBrowserChildProcessObserver() = default;

  // Returns the notifications received for |child_id|.
  const std::vector<Notification>& notifications() const {
    return notifications_;
  }

 private:
  void OnNotification(Notification notification) {
    notifications_.push_back(notification);
  }

  BrowserChildProcessNotificationObserver inner_observer_;

  std::vector<Notification> notifications_;
};

class BrowserChildProcessObserverBrowserTest : public ContentBrowserTest {};

// Tests that launching and then using ForceShutdown() results in a normal
// termination.
#if defined(ADDRESS_SANITIZER)
// TODO(crbug.com/40238612): Fix ASAN failures on trybot.
#define MAYBE_LaunchAndForceShutdown DISABLED_LaunchAndForceShutdown
#else
#define MAYBE_LaunchAndForceShutdown LaunchAndForceShutdown
#endif
IN_PROC_BROWSER_TEST_F(BrowserChildProcessObserverBrowserTest,
                       MAYBE_LaunchAndForceShutdown) {
  base::WeakPtr<TestProcessHost> host = TestProcessHost::Create();
  int child_id = host->GetId();

  TestBrowserChildProcessObserver observer(child_id);

  {
    WaitForNotificationObserver waiter(child_id,
                                       Notification::kLaunchedAndConnected);
    host->LaunchProcess();
    waiter.Wait();
  }

  {
    WaitForNotificationObserver waiter(child_id, Notification::kDisconnected);
    host->ForceShutdown();
    waiter.Wait();
  }

  Notification kExitNotification =
#if BUILDFLAG(IS_ANDROID)
      // TODO(pmonette): On Android, this currently causes a killed
      // notification. Consider fixing.
      Notification::kKilled;
#else
      Notification::kExitedNormally;
#endif  // BUILDFLAG(IS_ANDROID)

  // The host should be deleted now.
  EXPECT_FALSE(host);
  EXPECT_FALSE(IsHostAlive(child_id));
  EXPECT_THAT(observer.notifications(),
              testing::ElementsAreArray({Notification::kLaunchedAndConnected,
                                         kExitNotification,
                                         Notification::kDisconnected}));
}

// Tests that launching and then deleting the host results in a normal
// termination.
IN_PROC_BROWSER_TEST_F(BrowserChildProcessObserverBrowserTest,
                       LaunchAndDelete) {
  base::WeakPtr<TestProcessHost> host = TestProcessHost::Create();
  int child_id = host->GetId();

  TestBrowserChildProcessObserver observer(child_id);

  {
    WaitForNotificationObserver waiter(child_id,
                                       Notification::kLaunchedAndConnected);
    host->LaunchProcess();
    waiter.Wait();
  }

  {
    WaitForNotificationObserver waiter(child_id, Notification::kDisconnected);
    delete host.get();
    waiter.Wait();
  }

  // The host should be deleted now.
  EXPECT_FALSE(host);
  EXPECT_FALSE(IsHostAlive(child_id));
  EXPECT_THAT(observer.notifications(),
              testing::ElementsAreArray({Notification::kLaunchedAndConnected,
                                         Notification::kExitedNormally,
                                         Notification::kDisconnected}));
}

// Tests that launching and then disconnecting the service channel results in a
// normal termination.
// Note: This only works for services bound using BindServiceInterface(), not
// BindReceiver().
#if defined(ADDRESS_SANITIZER)
// TODO(crbug.com/40238612): Fix ASAN failures on trybot.
#define MAYBE_LaunchAndDisconnect DISABLED_LaunchAndDisconnect
#else
#define MAYBE_LaunchAndDisconnect LaunchAndDisconnect
#endif
IN_PROC_BROWSER_TEST_F(BrowserChildProcessObserverBrowserTest,
                       MAYBE_LaunchAndDisconnect) {
  base::WeakPtr<TestProcessHost> host = TestProcessHost::Create();
  int child_id = host->GetId();

  TestBrowserChildProcessObserver observer(child_id);

  {
    WaitForNotificationObserver waiter(child_id,
                                       Notification::kLaunchedAndConnected);
    host->LaunchProcess();
    waiter.Wait();
  }

  {
    WaitForNotificationObserver waiter(child_id, Notification::kDisconnected);
    host->Disconnect();
    waiter.Wait();
  }

  Notification kExitNotification =
#if BUILDFLAG(IS_ANDROID)
      // On Android, kKilled is always sent in the case of a crash.
      Notification::kKilled;
#else
      Notification::kExitedNormally;
#endif  // BUILDFLAG(IS_ANDROID)

  // The host should be deleted now.
  EXPECT_FALSE(host);
  EXPECT_FALSE(IsHostAlive(child_id));
  EXPECT_THAT(observer.notifications(), testing::ElementsAreArray({
                                            Notification::kLaunchedAndConnected,
                                            kExitNotification,
                                            Notification::kDisconnected,
                                        }));
}

// Tests that launching and then causing a crash the host results in a crashed
// notification.
// TODO(crbug.com/40868150): Times out on Android tests.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_LaunchAndCrash DISABLED_LaunchAndCrash
#else
#define MAYBE_LaunchAndCrash LaunchAndCrash
#endif
IN_PROC_BROWSER_TEST_F(BrowserChildProcessObserverBrowserTest,
                       MAYBE_LaunchAndCrash) {
  base::WeakPtr<TestProcessHost> host = TestProcessHost::Create();
  int child_id = host->GetId();

  TestBrowserChildProcessObserver observer(child_id);

  {
    WaitForNotificationObserver waiter(child_id,
                                       Notification::kLaunchedAndConnected);
    host->LaunchProcess();
    waiter.Wait();
  }

  {
    WaitForNotificationObserver waiter(child_id, Notification::kDisconnected);
    host->service()->DoCrashImmediately(base::DoNothing());
    waiter.Wait();
  }

  Notification kCrashedNotification =
#if BUILDFLAG(IS_ANDROID)
      // On Android, kKilled is always sent in the case of a crash.
      Notification::kKilled;
#else
      Notification::kCrashed;
#endif  // BUILDFLAG(IS_ANDROID)

  // The host should be deleted now.
  EXPECT_FALSE(host);
  EXPECT_FALSE(IsHostAlive(child_id));
  EXPECT_THAT(observer.notifications(),
              testing::ElementsAreArray({Notification::kLaunchedAndConnected,
                                         kCrashedNotification,
                                         Notification::kDisconnected}));
}

// Tests that kLaunchFailed is correctly sent when the child process fails to
// launch.
//
// This test won't work as-is on POSIX platforms, where fork()+exec() is used to
// launch child processes, failure does not happen until exec(), therefore the
// test will see a valid child process followed by a
// TERMINATION_STATUS_ABNORMAL_TERMINATION of the forked process. However,
// posix_spawn() is used on macOS.
// See also ServiceProcessLauncherTest.FailToLaunchProcess.
#if !BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(BrowserChildProcessObserverBrowserTest, LaunchFailed) {
  base::WeakPtr<TestProcessHost> host = TestProcessHost::Create();
  int child_id = host->GetId();

#if BUILDFLAG(IS_WIN)
  // The Windows sandbox does not like the child process being a different
  // process, so launch unsandboxed for the purpose of this test.
  host->SetSandboxType(sandbox::mojom::Sandbox::kNoSandbox);
#endif

  // Simulate a catastrophic launch failure for all child processes by
  // making the path to the process non-existent.
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kBrowserSubprocessPath,
      base::FilePath(FILE_PATH_LITERAL("non_existent_path")));

  TestBrowserChildProcessObserver observer(child_id);

  {
    WaitForNotificationObserver waiter(child_id, Notification::kLaunchFailed);
    host->LaunchProcess();
    waiter.Wait();
  }

  // The host should be deleted now.
  EXPECT_FALSE(host);
  EXPECT_FALSE(IsHostAlive(child_id));
  EXPECT_THAT(observer.notifications(),
              testing::ElementsAreArray({Notification::kLaunchFailed}));
}
#endif  // !BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
class TestPreSpawnTargetFailureSandboxedProcessLauncherDelegate
    : public TestSandboxedProcessLauncherDelegate {
 public:
  using TestSandboxedProcessLauncherDelegate::
      TestSandboxedProcessLauncherDelegate;

  // SandboxedProcessLauncherDelegate:
  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
    // Force a failure in PreSpawnTarget().
    return false;
  }
};

// Override the observer to verify the error occurred in PreSpawnTarget().
class TestPreSpawnTargetFailureBrowserChildProcessNotificationObserver
    : public BrowserChildProcessNotificationObserver {
 public:
  using BrowserChildProcessNotificationObserver::
      BrowserChildProcessNotificationObserver;

  // BrowserChildProcessObserver:
  void BrowserChildProcessLaunchFailed(
      const ChildProcessData& data,
      const ChildProcessTerminationInfo& info) override {
    EXPECT_EQ(info.exit_code, sandbox::SBOX_ERROR_DELEGATE_PRE_SPAWN);
    BrowserChildProcessNotificationObserver::OnNotification(
        data, Notification::kLaunchFailed);
  }
};

// Tests that a pre spawn failure results in a failed launch.
IN_PROC_BROWSER_TEST_F(BrowserChildProcessObserverBrowserTest,
                       LaunchPreSpawnFailed) {
  base::WeakPtr<TestProcessHost> host = TestProcessHost::Create();
  int child_id = host->GetId();

  TestBrowserChildProcessObserver observer(child_id);

  {
    WaitForNotificationObserver waiter(child_id, Notification::kLaunchFailed);
    host->LaunchProcessWithDelegate(
        std::make_unique<
            TestPreSpawnTargetFailureSandboxedProcessLauncherDelegate>(
            sandbox::mojom::Sandbox::kUtility));
    waiter.Wait();
  }

  // The host should be deleted now.
  EXPECT_FALSE(host);
  EXPECT_FALSE(IsHostAlive(child_id));
  EXPECT_THAT(observer.notifications(),
              testing::ElementsAreArray({Notification::kLaunchFailed}));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
