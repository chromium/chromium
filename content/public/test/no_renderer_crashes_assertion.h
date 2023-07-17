// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_NO_RENDERER_CRASHES_ASSERTION_H_
#define CONTENT_PUBLIC_TEST_NO_RENDERER_CRASHES_ASSERTION_H_

#include <map>

#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "base/scoped_multi_source_observation.h"
#include "base/sequence_checker.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
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

  ScopedAllowRendererCrashes(const ScopedAllowRendererCrashes&) = delete;
  ScopedAllowRendererCrashes& operator=(const ScopedAllowRendererCrashes&) =
      delete;

  ~ScopedAllowRendererCrashes();

 private:
  // The id of the process that is allowed to crash while this
  // ScopedAllowRendererCrashes object is alive.
  //
  // The special |ChildProcessHost::kInvalidUniqueID| value means that crashes
  // of *any* process are allowed.
  int process_id_;
};

// Helper that BrowserTestBase can use to start monitoring for renderer crashes
// (triggering a test failure when a renderer crash happens).
class NoRendererCrashesAssertion : public RenderProcessHostCreationObserver,
                                   public RenderProcessHostObserver {
 public:
  NoRendererCrashesAssertion();

  NoRendererCrashesAssertion(const NoRendererCrashesAssertion&) = delete;
  NoRendererCrashesAssertion& operator=(const NoRendererCrashesAssertion&) =
      delete;

  ~NoRendererCrashesAssertion() override;

 private:
  // RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(RenderProcessHost* host) override;

  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  base::ScopedMultiSourceObservation<RenderProcessHost,
                                     RenderProcessHostObserver>
      process_observations_{this};

  // Internal helper class for keeping track of suspensions that should cause
  // us to ignore crashes in a specific renderer process.
  class Suspensions {
   public:
    static Suspensions& GetInstance();
    Suspensions();

    Suspensions(const Suspensions&) = delete;
    Suspensions& operator=(const Suspensions&) = delete;

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
  };
  friend ScopedAllowRendererCrashes;
  FRIEND_TEST_ALL_PREFIXES(NoRendererCrashesAssertionSuspensions,
                           SingleProcess);
  FRIEND_TEST_ALL_PREFIXES(NoRendererCrashesAssertionSuspensions, AllProcesses);
  FRIEND_TEST_ALL_PREFIXES(NoRendererCrashesAssertionSuspensions,
                           SingleProcessNesting);
  FRIEND_TEST_ALL_PREFIXES(NoRendererCrashesAssertionSuspensions,
                           AllProcessesNesting);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_NO_RENDERER_CRASHES_ASSERTION_H_
