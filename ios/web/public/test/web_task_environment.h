// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_WEB_TASK_ENVIRONMENT_H_
#define IOS_WEB_PUBLIC_TEST_WEB_TASK_ENVIRONMENT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/test/task_environment.h"
#include "base/traits_bag.h"

// WebTaskEnvironment is the iOS equivalent of content::BrowserTaskEnvironment.
//
// It's is a convenience class for creating a set of TestWebThreads and a thread
// pool in unit tests. For most tests, it is sufficient to just instantiate the
// WebTaskEnvironment as a member variable. It is a good idea to put the
// WebTaskEnvironment as the first member variable in test classes, so it is
// destroyed last, and the test threads always exist from the perspective of
// other classes.
//
// By default, all of the created TestWebThreads will be backed by a single
// shared MessageLoop. If a test truly needs separate threads, it can do so by
// passing the appropriate combination of option values during the
// WebTaskEnvironment construction.
//
// To synchronously run tasks posted to TestWebThreads that use the shared
// MessageLoop, call RunLoop::Run/RunUntilIdle() on the thread where the
// WebTaskEnvironment lives. The destructor of WebTaskEnvironment runs remaining
// TestWebThreads tasks and remaining BLOCK_SHUTDOWN thread pool tasks.
//
// Some tests using the IO thread expect a MessageLoopForIO. Passing
// MainThreadType::IO will use a MessageLoopForIO for the main MessageLoop.
// Most of the time, this avoids needing to use a IOThreadType::REAL_THREAD.

namespace web {

class TestWebThread;

class WebTaskEnvironment : public base::test::TaskEnvironment {
 public:
  // This type will determine which events will be pumped by the main
  // thread. Note that the default is different from TaskEnvironment.
  enum class MainThreadType {
    UI,
    IO,
    DEFAULT = UI,
  };

  // This type will determine whether the IO thread is backed by a real
  // thread or not (DEFAULT).
  enum class IOThreadType {
    FAKE_THREAD,
    REAL_THREAD,

    // If used, the IO thread is backed by a real thread but is not started
    // automatically. Instead, user must call StartIOThread() to start it.
    REAL_THREAD_DELAYED,
    DEFAULT = FAKE_THREAD,
  };

  // List of traits that are valid inputs for the constructor below.
  struct ValidTraits {
    ValidTraits(TimeSource);
    ValidTraits(IOThreadType);
    ValidTraits(MainThreadType);
  };

  // Constructor accepts zero or more traits which customize the environment.
  template <typename... WebTaskEnvironmentTraits>
    requires base::trait_helpers::AreValidTraits<ValidTraits,
                                                 WebTaskEnvironmentTraits...>
  NOINLINE explicit WebTaskEnvironment(WebTaskEnvironmentTraits... traits)
      : WebTaskEnvironment(
            base::trait_helpers::GetEnum<TimeSource,  //
                                         TimeSource::DEFAULT>(traits...),
            base::trait_helpers::GetEnum<MainThreadType,  //
                                         MainThreadType::DEFAULT>(traits...),
            base::trait_helpers::GetEnum<IOThreadType,  //
                                         IOThreadType::DEFAULT>(traits...),
            base::trait_helpers::NotATraitTag()) {}

  WebTaskEnvironment(const WebTaskEnvironment&) = delete;
  WebTaskEnvironment& operator=(const WebTaskEnvironment&) = delete;

  ~WebTaskEnvironment() override;

  // Starts the IO thread. This method must only be called if IOThreadType was
  // initialized with REAL_THREAD_DELAYED during construction.
  void StartIOThread();

 private:
  // The template constructor has to be in the header but it delegates to this
  // constructor to initialize all other members out-of-line.
  WebTaskEnvironment(TimeSource time_source,
                     MainThreadType main_thread_type,
                     IOThreadType io_thread_type,
                     base::trait_helpers::NotATraitTag tag);

  // Starts the IO thread.
  void StartIOThreadInternal();

  const IOThreadType io_thread_type_;
  std::unique_ptr<TestWebThread> ui_thread_;
  std::unique_ptr<TestWebThread> io_thread_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_WEB_TASK_ENVIRONMENT_H_
