// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/observed_service_remote.h"

#include "base/run_loop.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "services/test/echo/public/mojom/echo.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
constexpr char kTestUrl[] = "https://foo.bar";
}  // namespace

using ObservedServiceRemoteBrowserTest = ContentBrowserTest;

// Verifies ObservedServiceRemote couples the remote and observer lifecycle:
// observers are notified on launch (including late registration), on process
// death, and stale state is cleared after termination.
IN_PROC_BROWSER_TEST_F(ObservedServiceRemoteBrowserTest, Lifecycle) {
  ObservedServiceRemote<echo::mojom::EchoService> observed;

  class TestObserver
      : public ObservedServiceRemote<echo::mojom::EchoService>::Observer {
   public:
    void OnServiceLaunched(const ServiceProcessInfo& info) override {
      launch_count_++;
      if (launch_quit_) {
        std::move(launch_quit_).Run();
      }
    }
    void OnServiceTerminatedNormally(const ServiceProcessInfo& info) override {
      terminate_count_++;
      if (terminate_quit_) {
        std::move(terminate_quit_).Run();
      }
    }
    void OnServiceCrashed(const ServiceProcessInfo& info) override {
      crash_count_++;
      if (crash_quit_) {
        std::move(crash_quit_).Run();
      }
    }

    int launch_count() const { return launch_count_; }
    int terminate_count() const { return terminate_count_; }
    int crash_count() const { return crash_count_; }

    void set_launch_quit(base::OnceClosure quit) {
      launch_quit_ = std::move(quit);
    }
    void set_terminate_quit(base::OnceClosure quit) {
      terminate_quit_ = std::move(quit);
    }
    void set_crash_quit(base::OnceClosure quit) {
      crash_quit_ = std::move(quit);
    }

   private:
    int launch_count_ = 0;
    int terminate_count_ = 0;
    int crash_count_ = 0;
    base::OnceClosure launch_quit_;
    base::OnceClosure terminate_quit_;
    base::OnceClosure crash_quit_;
  };

  TestObserver observer;
  observed.AddObserver(&observer);

  base::RunLoop launch_loop;
  observer.set_launch_quit(launch_loop.QuitClosure());
  ServiceProcessHost::Launch(observed);
  launch_loop.Run();
  EXPECT_EQ(1, observer.launch_count());

  // Late observer registered while running should be notified immediately.
  TestObserver late_while_running;
  observed.AddObserver(&late_while_running);
  EXPECT_EQ(1, late_while_running.launch_count());

  // Reset the remote — triggers normal termination notification.
  base::RunLoop death_loop;
  observer.set_terminate_quit(death_loop.QuitClosure());
  observed.remote().reset();
  death_loop.Run();
  EXPECT_EQ(1, observer.terminate_count());

  // After death, a late observer should NOT get a stale launch notification.
  TestObserver late_after_death;
  observed.AddObserver(&late_after_death);
  EXPECT_EQ(0, late_after_death.launch_count());

  // Re-launch — observers should see a second launch.
  base::RunLoop launch2;
  observer.set_launch_quit(launch2.QuitClosure());
  ServiceProcessHost::Launch(observed);
  launch2.Run();
  EXPECT_EQ(2, observer.launch_count());
  EXPECT_EQ(1, late_after_death.launch_count());

  TestObserver late_during_second;
  observed.AddObserver(&late_during_second);
  EXPECT_EQ(1, late_during_second.launch_count());

  base::RunLoop death2;
  observer.set_terminate_quit(death2.QuitClosure());
  observed.remote().reset();
  death2.Run();
  EXPECT_EQ(2, observer.terminate_count());

  observed.RemoveObserver(&observer);
  observed.RemoveObserver(&late_while_running);
  observed.RemoveObserver(&late_after_death);
  observed.RemoveObserver(&late_during_second);
}

// Verifies ObservedServiceRemote notifies observers on process crash.
IN_PROC_BROWSER_TEST_F(ObservedServiceRemoteBrowserTest, Crash) {
  ObservedServiceRemote<echo::mojom::EchoService> observed;

  class CrashObserver
      : public ObservedServiceRemote<echo::mojom::EchoService>::Observer {
   public:
    void OnServiceLaunched(const ServiceProcessInfo& info) override {
      launch_count_++;
      if (launch_quit_) {
        std::move(launch_quit_).Run();
      }
    }
    void OnServiceCrashed(const ServiceProcessInfo& info) override {
      crash_count_++;
      if (crash_quit_) {
        std::move(crash_quit_).Run();
      }
    }

    int launch_count() const { return launch_count_; }
    int crash_count() const { return crash_count_; }
    void set_launch_quit(base::OnceClosure quit) {
      launch_quit_ = std::move(quit);
    }
    void set_crash_quit(base::OnceClosure quit) {
      crash_quit_ = std::move(quit);
    }

   private:
    int launch_count_ = 0;
    int crash_count_ = 0;
    base::OnceClosure launch_quit_;
    base::OnceClosure crash_quit_;
  };

  CrashObserver observer;
  observed.AddObserver(&observer);

  base::RunLoop launch_loop;
  observer.set_launch_quit(launch_loop.QuitClosure());
  ServiceProcessHost::Launch(
      observed, ServiceProcessHost::Options().WithSite(GURL(kTestUrl)).Pass());
  launch_loop.Run();
  EXPECT_EQ(1, observer.launch_count());

  base::RunLoop crash_loop;
  observer.set_crash_quit(crash_loop.QuitClosure());
  observed.remote()->Crash();
  crash_loop.Run();
  EXPECT_EQ(1, observer.crash_count());

  // Re-launch after crash — use a fresh ObservedServiceRemote since the old
  // remote is still bound to the broken pipe (same as the old test pattern
  // where each Launch() returned a new Remote).
  ObservedServiceRemote<echo::mojom::EchoService> observed2;
  observed2.AddObserver(&observer);

  base::RunLoop relaunch_loop;
  observer.set_launch_quit(relaunch_loop.QuitClosure());
  ServiceProcessHost::Launch(
      observed2, ServiceProcessHost::Options().WithSite(GURL(kTestUrl)).Pass());
  relaunch_loop.Run();
  EXPECT_EQ(2, observer.launch_count());

  observed2.RemoveObserver(&observer);
  observed.RemoveObserver(&observer);
}

// Verifies that the same ObservedServiceRemote can be relaunched after a crash
// when reset_on_disconnect() is used — matching the production audio service
// pattern.
IN_PROC_BROWSER_TEST_F(ObservedServiceRemoteBrowserTest,
                       CrashAndRelaunchSameInstance) {
  ObservedServiceRemote<echo::mojom::EchoService> observed;

  class TestObserver
      : public ObservedServiceRemote<echo::mojom::EchoService>::Observer {
   public:
    void OnServiceLaunched(const ServiceProcessInfo& info) override {
      launch_count_++;
      if (launch_quit_) {
        std::move(launch_quit_).Run();
      }
    }
    void OnServiceCrashed(const ServiceProcessInfo& info) override {
      crash_count_++;
      if (crash_quit_) {
        std::move(crash_quit_).Run();
      }
    }

    int launch_count_ = 0;
    int crash_count_ = 0;
    base::OnceClosure launch_quit_;
    base::OnceClosure crash_quit_;
  };

  TestObserver observer;
  observed.AddObserver(&observer);

  // Launch and set a disconnect handler that resets the remote (matching the
  // production reset_on_disconnect pattern, but also signaling completion so
  // we can reliably wait for it in the test).
  base::RunLoop launch_loop;
  observer.launch_quit_ = launch_loop.QuitClosure();
  ServiceProcessHost::Launch(
      observed, ServiceProcessHost::Options().WithSite(GURL(kTestUrl)).Pass());
  launch_loop.Run();
  EXPECT_EQ(1, observer.launch_count_);

  base::RunLoop disconnect_loop;
  observed.remote().set_disconnect_handler(base::BindOnce(
      [](mojo::Remote<echo::mojom::EchoService>* remote,
         base::OnceClosure quit) {
        remote->reset();
        std::move(quit).Run();
      },
      base::Unretained(&observed.remote()), disconnect_loop.QuitClosure()));

  // Crash the service.
  base::RunLoop crash_loop;
  observer.crash_quit_ = crash_loop.QuitClosure();
  observed.remote()->Crash();
  crash_loop.Run();
  EXPECT_EQ(1, observer.crash_count_);

  // Wait for the disconnect handler to reset the remote.
  disconnect_loop.Run();
  EXPECT_FALSE(observed.remote().is_bound());

  // Re-launch on the SAME instance — proves crash-restart safety.
  base::RunLoop relaunch_loop;
  observer.launch_quit_ = relaunch_loop.QuitClosure();
  ServiceProcessHost::Launch(
      observed, ServiceProcessHost::Options().WithSite(GURL(kTestUrl)).Pass());
  relaunch_loop.Run();
  EXPECT_EQ(2, observer.launch_count_);

  // Late observer on relaunched instance should see it running.
  TestObserver late;
  observed.AddObserver(&late);
  EXPECT_EQ(1, late.launch_count_);

  observed.RemoveObserver(&observer);
  observed.RemoveObserver(&late);
}

}  // namespace content
