/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008 Apple Inc.  All right reserved.
 * Copyright (C) 2011 Google, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_BIDI_RUN_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_BIDI_RUN_LIST_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

template <class Run>
class BidiRunList final {
  DISALLOW_NEW();

 public:
  BidiRunList()
      : first_run_(nullptr),
        last_run_(nullptr),
        logically_last_run_(nullptr),
        run_count_(0) {}

  // FIXME: Once BidiResolver no longer owns the BidiRunList,
  // then ~BidiRunList should call deleteRuns() automatically.

  Run* FirstRun() const { return first_run_; }
  Run* LastRun() const { return last_run_; }
  Run* LogicallyLastRun() const { return logically_last_run_; }
  unsigned RunCount() const { return run_count_; }

  void AddRun(Run*);
  void PrependRun(Run*);

  void MoveRunToEnd(Run*);
  void MoveRunToBeginning(Run*);

  void DeleteRuns();
  void ReverseRuns(unsigned start, unsigned end);
  void ReorderRunsFromLevels();

  void SetLogicallyLastRun(Run* run) { logically_last_run_ = run; }

  void ReplaceRunWithRuns(Run* to_replace, BidiRunList<Run>& new_runs);

 private:
  void ClearWithoutDestroyingRuns();

  Run* first_run_;
  Run* last_run_;
  Run* logically_last_run_;
  unsigned run_count_;

  DISALLOW_COPY_AND_ASSIGN(BidiRunList);
};

template <class Run>
inline void BidiRunList<Run>::AddRun(Run* run) {
  if (!first_run_)
    first_run_ = run;
  else
    last_run_->next_ = run;
  last_run_ = run;
  run_count_++;
}

template <class Run>
inline void BidiRunList<Run>::PrependRun(Run* run) {
  DCHECK(!run->next_);

  if (!last_run_)
    last_run_ = run;
  else
    run->next_ = first_run_;
  first_run_ = run;
  run_count_++;
}

template <class Run>
inline void BidiRunList<Run>::MoveRunToEnd(Run* run) {
  DCHECK(first_run_);
  DCHECK(last_run_);
  DCHECK(run->next_);

  Run* current = nullptr;
  Run* next = first_run_;
  while (next != run) {
    current = next;
    next = current->Next();
  }

  if (!current)
    first_run_ = run->Next();
  else
    current->next_ = run->next_;

  run->next_ = nullptr;
  last_run_->next_ = run;
  last_run_ = run;
}

template <class Run>
inline void BidiRunList<Run>::MoveRunToBeginning(Run* run) {
  DCHECK(first_run_);
  DCHECK(last_run_);
  DCHECK_NE(run, first_run_);

  Run* current = first_run_;
  Run* next = current->Next();
  while (next != run) {
    current = next;
    next = current->Next();
  }

  current->next_ = run->next_;
  if (run == last_run_)
    last_run_ = current;

  run->next_ = first_run_;
  first_run_ = run;
}

template <class Run>
void BidiRunList<Run>::ReplaceRunWithRuns(Run* to_replace,
                                          BidiRunList<Run>& new_runs) {
  DCHECK(new_runs.RunCount());
  DCHECK(first_run_);
  DCHECK(to_replace);

  if (first_run_ == to_replace) {
    first_run_ = new_runs.FirstRun();
  } else {
    // Find the run just before "toReplace" in the list of runs.
    Run* previous_run = first_run_;
    while (previous_run->Next() != to_replace)
      previous_run = previous_run->Next();
    DCHECK(previous_run);
    previous_run->SetNext(new_runs.FirstRun());
  }

  new_runs.LastRun()->SetNext(to_replace->Next());

  // Fix up any of other pointers which may now be stale.
  if (last_run_ == to_replace)
    last_run_ = new_runs.LastRun();
  if (logically_last_run_ == to_replace)
    logically_last_run_ = new_runs.LogicallyLastRun();
  run_count_ +=
      new_runs.RunCount() - 1;  // We added the new runs and removed toReplace.

  delete to_replace;
  new_runs.ClearWithoutDestroyingRuns();
}

template <class Run>
void BidiRunList<Run>::ClearWithoutDestroyingRuns() {
  first_run_ = nullptr;
  last_run_ = nullptr;
  logically_last_run_ = nullptr;
  run_count_ = 0;
}

template <class Run>
void BidiRunList<Run>::DeleteRuns() {
  if (!first_run_)
    return;

  Run* curr = first_run_;
  while (curr) {
    Run* s = curr->Next();
    delete curr;
    curr = s;
  }

  ClearWithoutDestroyingRuns();
}

template <class Run>
void BidiRunList<Run>::ReverseRuns(unsigned start, unsigned end) {
  DCHECK(run_count_);
  if (start >= end)
    return;

  DCHECK_LT(end, run_count_);

  // Get the item before the start of the runs to reverse and put it in
  // |beforeStart|. |curr| should point to the first run to reverse.
  Run* curr = first_run_;
  Run* before_start = nullptr;
  unsigned i = 0;
  while (i < start) {
    i++;
    before_start = curr;
    curr = curr->Next();
  }

  Run* start_run = curr;
  while (i < end) {
    i++;
    curr = curr->Next();
  }
  Run* end_run = curr;
  Run* after_end = curr->Next();

  i = start;
  curr = start_run;
  Run* new_next = after_end;
  while (i <= end) {
    // Do the reversal.
    Run* next = curr->Next();
    curr->next_ = new_next;
    new_next = curr;
    curr = next;
    i++;
  }

  // Now hook up beforeStart and afterEnd to the startRun and endRun.
  if (before_start)
    before_start->next_ = end_run;
  else
    first_run_ = end_run;

  start_run->next_ = after_end;
  if (!after_end)
    last_run_ = start_run;
}

}  // namespace blink

#endif  // BidiRunList
