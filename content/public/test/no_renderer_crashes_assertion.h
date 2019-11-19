// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_NO_RENDERER_CRASHES_ASSERTION_H_
#define CONTENT_PUBLIC_TEST_NO_RENDERER_CRASHES_ASSERTION_H_

#include <map>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"

namespace content {

class RenderProcessHost;

// Usually, a test should fail when BrowserTestBase detects a crash in a
// renderer process (see the NoRendererCrashesAssertion helper class below).
// The ScopedAllowRendererCrashes class can be used to temporarily suspend this
// behavior - this is useful in tests that explicitly expect renderer kills or
// crashes.
class ScopedAllowRendererCrashes {
 public:
  // Ignores *all* renderer crashes.
  ScopedAllowRendererCrashes();

  // Ignores crashes of |process|.
  explicit ScopedAllowRendererCrashes(RenderProcessHost* process);

  // Ignores crashes of the process associated with the given |frame|.
  explicit ScopedAllowRendererCrashes(const ToRenderFrameHost& frame);

  ~ScopedAllowRendererCrashes();

 private:
  // The id of the process that is allowed to crash while this
  // ScopedAllowRendererCrashes object is alive.
  //
  // The special |ChildProcessHost::kInvalidUniqueID| value means that crashes
  // of *any* process are allowed.
  int process_id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAllowRendererCrashes);
};

// Helper that BrowserTestBase can use to start monitoring for renderer crashes
// (triggering a test failure when a renderer crash happens).
//
// TODO(lukasza): https://crbug.com/972220: Actually start using this class,
// by constructing it from BrowserTestBase::ProxyRunTestOnMainThreadLoop
// (before calling PreRunTestOnMainThread).
class NoRendererCrashesAssertion : public NotificationObserver {
 public:
  NoRendererCrashesAssertion();
  ~NoRendererCrashesAssertion() override;

 private:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  NotificationRegistrar registrar_;

  // Internal helper class for keeping track of suspensions that should cause
  // us to ignore crashes in a specific renderer process.
  class Suspensions {
   public:
    static Suspensions& GetInstance();
    Suspensions();
    ~Suspensions();

    // Methods for adding or removing a suspension.
    //
    // The special |ChildProcessHost::kInvalidUniqueID| value means that crashes
    // of *any* process are allowed.
    void AddSuspension(int process_id);
    void RemoveSuspension(int process_id);

    // Checks whether |process_id| has an active suspension.
    bool IsSuspended(int process_id);

   private:
    friend class base::NoDestructor<Suspensions>;

    std::map<int, int> process_id_to_suspension_count_;
    SEQUENCE_CHECKER(sequence_checker_);

    DISALLOW_COPY_AND_ASSIGN(Suspensions);
  };
  friend ScopedAllowRendererCrashes;
  FRIEND_TEST_ALL_PREFIXES(NoRendererCrashesAssertionSuspensions,
                           SingleProcess);
  FRIEND_TEST_ALL_PREFIXES(NoRendererCrashesAssertionSuspensions, AllProcesses);
  FRIEND_TEST_ALL_PREFIXES(NoRendererCrashesAssertionSuspensions,
                           SingleProcessNesting);
  FRIEND_TEST_ALL_PREFIXES(NoRendererCrashesAssertionSuspensions,
                           AllProcessesNesting);

  DISALLOW_COPY_AND_ASSIGN(NoRendererCrashesAssertion);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_NO_RENDERER_CRASHES_ASSERTION_H_
