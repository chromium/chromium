/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
 */

#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

unsigned g_suspension_count = 0;

}  // namespace

ScopedPagePauser::ScopedPagePauser(Page* primary_page) {
  if (++g_suspension_count > 1) {
    return;
  }

  SetPaused(primary_page, true);
  pause_handle_ = ThreadScheduler::Current()->ToMainThreadScheduler()
                      ? ThreadScheduler::Current()
                            ->ToMainThreadScheduler()
                            ->PauseScheduler()
                      : nullptr;
}

ScopedPagePauser::ScopedPagePauser() : ScopedPagePauser(nullptr) {}

ScopedPagePauser::~ScopedPagePauser() {
  if (--g_suspension_count > 0) {
    return;
  }

  SetPaused(nullptr, false);
}

void ScopedPagePauser::SetPaused(Page* primary_page, bool paused) {
  // Make a copy of the collection. Undeferring loads can cause script to run,
  // which would mutate ordinaryPages() in the middle of iteration.
  HeapVector<Member<Page>> pages(Page::OrdinaryPages());

  for (const auto& page : pages) {
    page->SetShowPausedHudOverlay(primary_page && page != primary_page);
    page->SetPaused(paused);
  }
}

bool ScopedPagePauser::IsActive() {
  return g_suspension_count > 0;
}

}  // namespace blink
