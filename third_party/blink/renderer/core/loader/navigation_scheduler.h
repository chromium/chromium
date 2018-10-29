/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2009 Adam Barth. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_NAVIGATION_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_NAVIGATION_SCHEDULER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class FormSubmission;
class LocalFrame;
class ScheduledNavigation;

class CORE_EXPORT NavigationScheduler final
    : public GarbageCollectedFinalized<NavigationScheduler> {
 public:
  static NavigationScheduler* Create(LocalFrame* frame) {
    return new NavigationScheduler(frame);
  }

  ~NavigationScheduler();

  bool LocationChangePending();
  bool IsNavigationScheduledWithin(double interval_in_seconds) const;

  void ScheduleRedirect(double delay, const KURL&, Document::HttpRefreshType);
  void ScheduleFrameNavigation(Document*, const KURL&, WebFrameLoadType);
  void SchedulePageBlock(Document*, int reason);
  void ScheduleFormSubmission(Document*, FormSubmission*);
  void ScheduleReload();

  void StartTimer();
  void Cancel();

  void Trace(blink::Visitor*);

 private:
  explicit NavigationScheduler(LocalFrame*);

  bool ShouldScheduleReload() const;
  bool ShouldScheduleNavigation(const KURL&) const;

  void NavigateTask();
  void Schedule(ScheduledNavigation*);

  static bool MustReplaceCurrentItem(LocalFrame* target_frame);
  base::TimeTicks InputTimestamp();

  Member<LocalFrame> frame_;
  TaskHandle navigate_task_handle_;
  Member<ScheduledNavigation> redirect_;

  DISALLOW_COPY_AND_ASSIGN(NavigationScheduler);
};

class NavigationDisablerForBeforeUnload {
  DISALLOW_COPY_AND_ASSIGN(NavigationDisablerForBeforeUnload);
  STACK_ALLOCATED();

 public:
  NavigationDisablerForBeforeUnload() { navigation_disable_count_++; }
  ~NavigationDisablerForBeforeUnload() {
    DCHECK(navigation_disable_count_);
    navigation_disable_count_--;
  }
  static bool IsNavigationAllowed() { return !navigation_disable_count_; }

 private:
  static unsigned navigation_disable_count_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_NAVIGATION_SCHEDULER_H_
