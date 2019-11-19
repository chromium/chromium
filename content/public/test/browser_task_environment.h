// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BROWSER_TASK_ENVIRONMENT_H_
#define CONTENT_PUBLIC_TEST_BROWSER_TASK_ENVIRONMENT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"

namespace base {
#if defined(OS_WIN)
namespace win {
class ScopedCOMInitializer;
}  // namespace win
#endif
}  // namespace base

namespace content {

class TestBrowserThread;

// BrowserTaskEnvironment is a convenience class which allows usage of these
// APIs within its scope:
// - Same APIs as base::test::TaskEnvironment.
// - content::BrowserThread.
// - Public APIs of base::test::TaskEnvironment.
//
// Only tests that need the BrowserThread API should instantiate a
// BrowserTaskEnvironment. Use base::test::SingleThhreadTaskEnvironment or
// base::test::TaskEnvironment otherwise.
//
// By default, BrowserThread::UI/IO are backed by a single shared message loop
// on the main thread. If a test truly needs BrowserThread::IO tasks to run on a
// separate thread, it can pass the REAL_IO_THREAD option to the constructor.
// ThreadPool tasks always run on dedicated threads.
//
// To synchronously run tasks from the shared message loop:
//
// ... until there are no undelayed tasks in the shared message loop:
//    base::RunLoop::RunUntilIdle();
//
// ... until there are no undelayed tasks in the shared message loop, in
// ThreadPool (excluding tasks not posted from the shared message loop's thread
// or ThreadPool): task_environment.RunUntilIdle();
//
// ... until a condition is met:
//    base::RunLoop run_loop;
//    // Runs until a task running in the shared message loop calls
//    // run_loop.Quit() or runs run_loop.QuitClosure() (&run_loop or
//    // run_loop.QuitClosure() must be kept somewhere accessible by that task).
//    run_loop.Run();
//
// To wait until there are no pending undelayed tasks in ThreadPool, without
// running tasks from the shared message loop (this is rarely needed):
//    base::ThreadPoolInstance::Get()->FlushForTesting();
//
// The destructor of BrowserTaskEnvironment runs remaining UI/IO tasks and
// remaining thread pool tasks.
//
// If a test needs to pump IO messages on the main thread, it should use the
// IO_MAINLOOP option. Most of the time, IO_MAINLOOP avoids needing to use a
// REAL_IO_THREAD.
//
// For some tests it is important to emulate real browser startup. During real
// browser startup, the main message loop and the ThreadPool are created before
// browser threads. Passing DONT_CREATE_BROWSER_THREADS to the constructor will
// delay creating BrowserThreads until the test explicitly calls
// CreateBrowserThreads().
//
// DONT_CREATE_BROWSER_THREADS should only be used in conjunction with
// REAL_IO_THREAD.
//
// Basic usage:
//
//   class MyTestFixture : public testing::Test {
//    public:
//     (...)
//
//    protected:
//     // Must be the first member (or at least before any member that cares
//     // about tasks) to be initialized first and destroyed last. protected
//     // instead of private visibility will allow controlling the task
//     // environment (e.g. clock --see base::test::TaskEnvironment for
//     // details).
//     content::BrowserTaskEnvironment task_environment_;
//
//     // Other members go here (or further below in private section.)
//   };
//
// To add a BrowserTaskEnvironment to a ChromeFooBase test fixture when its
// FooBase base class already provides a base::test::TaskEnvironment:
//   class FooBase {
//    public:
//     // Constructs a FooBase with |traits| being forwarded to its
//     // TaskEnvironment.
//     template <typename... TaskEnvironmentTraits>
//     explicit FooBase(TaskEnvironmentTraits&&... traits)
//         : task_environment_(
//               base::in_place,
//               std::forward<TaskEnvironmentTraits>(traits)...) {}
//
//     // Alternatively a subclass may pass this tag to ask this FooBase not to
//     // instantiate a TaskEnvironment. The subclass is then responsible
//     // to instantiate one before FooBase::SetUp().
//     struct SubclassManagesTaskEnvironment {};
//     FooBase(SubclassManagesTaskEnvironment tag);
//
//    protected:
//     // Use this protected member directly from the test body to drive tasks
//     // posted within a FooBase-based test.
//     base::Optional<base::test::TaskEnvironment> task_environment_;
//   };
//
//   class ChromeFooBase : public FooBase {
//    public:
//     explicit ChromeFooBase(TaskEnvironmentTraits&&... traits)
//         : FooBase(FooBase::SubclassManagesTaskEnvironment()),
//           task_environment_(
//               std::forward<TaskEnvironmentTraits>(traits)...) {}
//
//    protected:
//     // Use this protected member directly to drive tasks posted within a
//     // ChromeFooBase-based test.
//     content::BrowserTaskEnvironment task_environment_;
//   };
// See views::ViewsTestBase / ChromeViewsTestBase for a real-world example.
class BrowserTaskEnvironment : public base::test::TaskEnvironment {
 public:
  enum Options { REAL_IO_THREAD };

  // The main thread will use a MessageLoopForIO (and support the
  // base::FileDescriptorWatcher API on POSIX).
  // TODO(alexclarke): Replace IO_MAINLOOP usage by MainThreadType::IO and
  // remove this.
  static constexpr MainThreadType IO_MAINLOOP = MainThreadType::IO;

  struct ValidTraits {
    ValidTraits(TaskEnvironment::ValidTraits);
    ValidTraits(Options);
  };

  // Constructor which accepts zero or more traits to configure the
  // TaskEnvironment and optionally request a real IO thread. Unlike
  // TaskEnvironment the default MainThreadType for
  // BrowserTaskEnvironment is MainThreadType::UI.
  template <
      typename... TaskEnvironmentTraits,
      class CheckArgumentsAreValid = std::enable_if_t<
          base::trait_helpers::AreValidTraits<ValidTraits,
                                              TaskEnvironmentTraits...>::value>>
  NOINLINE explicit BrowserTaskEnvironment(TaskEnvironmentTraits... traits)
      : BrowserTaskEnvironment(
            base::test::TaskEnvironment(
                SubclassCreatesDefaultTaskRunner{},
                base::trait_helpers::GetEnum<MainThreadType,
                                             MainThreadType::UI>(traits...),
                base::trait_helpers::Exclude<MainThreadType, Options>::Filter(
                    traits)...),
            UseRealIOThread(
                base::trait_helpers::GetOptionalEnum<Options>(traits...))) {}

  // Flush the IO thread. Replacement for RunLoop::RunUntilIdle() for tests that
  // have a REAL_IO_THREAD. As with TaskEnvironment::RunUntilIdle() prefer using
  // RunLoop+QuitClosure() to await an async condition.
  void RunIOThreadUntilIdle();

  ~BrowserTaskEnvironment() override;

 private:
  // The template constructor has to be in the header but it delegates to this
  // constructor to initialize all other members out-of-line.
  BrowserTaskEnvironment(base::test::TaskEnvironment&& scoped_task_environment,
                         bool real_io_thread);

  void Init();

  static constexpr bool UseRealIOThread(base::Optional<Options> options) {
    if (!options)
      return false;
    return *options == Options::REAL_IO_THREAD;
  }

  constexpr bool HasIOMainLoop() const {
    return main_thread_type() == MainThreadType::IO;
  }

  const bool real_io_thread_;
  std::unique_ptr<TestBrowserThread> ui_thread_;
  std::unique_ptr<TestBrowserThread> io_thread_;

#if defined(OS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BrowserTaskEnvironment);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BROWSER_TASK_ENVIRONMENT_H_
