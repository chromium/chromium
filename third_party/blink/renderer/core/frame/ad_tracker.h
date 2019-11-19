// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_TRACKER_H_

#include "base/feature_list.h"
#include "base/macros.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/probe/async_task_id.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {
class ExecutionContext;
class ResourceRequest;
enum class ResourceType : uint8_t;

namespace probe {
class CallFunction;
class ExecuteScript;
}  // namespace probe

namespace features {
CORE_EXPORT extern const base::Feature kAsyncStackAdTagging;
CORE_EXPORT extern const base::Feature kTopOfStackAdTagging;
}

// Tracker for tagging resources as ads based on the call stack scripts.
// The tracker is maintained per local root.
class CORE_EXPORT AdTracker : public GarbageCollected<AdTracker> {
 public:
  // Finds an AdTracker for a given ExecutionContext.
  static AdTracker* FromExecutionContext(ExecutionContext*);

  // Instrumenting methods.
  // Called when a script module or script gets executed from native code.
  void Will(const probe::ExecuteScript&);
  void Did(const probe::ExecuteScript&);

  // Called when a function gets called from native code.
  void Will(const probe::CallFunction&);
  void Did(const probe::CallFunction&);

  // Called when a subresource request is about to be sent or is redirected.
  // Returns true if:
  // - If the resource is loaded in an ad iframe
  // - If ad script is in the v8 stack
  // - |known_ad| is true
  // Virtual for testing.
  virtual bool CalculateIfAdSubresource(ExecutionContext* execution_context,
                                        const ResourceRequest& request,
                                        ResourceType resource_type,
                                        bool known_ad);

  // Called when an async task is created. Check at this point for ad script on
  // the stack and annotate the task if so.
  void DidCreateAsyncTask(probe::AsyncTaskId* task);

  // Called when an async task is eventually run.
  void DidStartAsyncTask(probe::AsyncTaskId* task);

  // Called when the task has finished running.
  void DidFinishAsyncTask(probe::AsyncTaskId* task);

  // Returns true if any script in the pseudo call stack has previously been
  // identified as an ad resource.
  bool IsAdScriptInStack();

  virtual void Trace(blink::Visitor*);

  void Shutdown();
  explicit AdTracker(LocalFrame*);
  virtual ~AdTracker();

 protected:
  // Protected for testing.
  virtual String ScriptAtTopOfStack(ExecutionContext*);
  virtual ExecutionContext* GetCurrentExecutionContext();

 private:
  friend class FrameFetchContextSubresourceFilterTest;
  friend class AdTrackerSimTest;
  friend class AdTrackerTest;

  void WillExecuteScript(ExecutionContext*, const String& script_name);
  void DidExecuteScript();
  bool IsKnownAdScript(ExecutionContext* execution_context, const String& url);
  void AppendToKnownAdScripts(ExecutionContext&, const String& url);

  Member<LocalFrame> local_root_;

  // Each time v8 is started to run a script or function, this records if it was
  // an ad script. Each time the script or function finishes, it pops the stack.
  Vector<bool> stack_frame_is_ad_;

  uint32_t num_ads_in_stack_ = 0;

  // The set of ad scripts detected outside of ad-frame contexts.
  HeapHashMap<WeakMember<ExecutionContext>, HashSet<String>> known_ad_scripts_;

  // The number of ad-related async tasks currently running in the stack.
  uint32_t running_ad_async_tasks_ = 0;

  const bool async_stack_enabled_;
  const bool top_of_stack_only_;

  DISALLOW_COPY_AND_ASSIGN(AdTracker);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_TRACKER_H_
