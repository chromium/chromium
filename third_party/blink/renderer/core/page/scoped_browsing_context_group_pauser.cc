// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scoped_browsing_context_group_pauser.h"

#include <limits>
#include <map>

#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

uint64_t& PausedCountPerBrowsingContextGroup(
    const base::UnguessableToken& token) {
  using BrowsingContextGroupMap = std::map<base::UnguessableToken, uint64_t>;
  DEFINE_STATIC_LOCAL(BrowsingContextGroupMap, counts, ());
  return counts[token];
}

}  // namespace

// static
bool ScopedBrowsingContextGroupPauser::IsActive(Page& page) {
  return PausedCountPerBrowsingContextGroup(page.BrowsingContextGroupToken()) >
         0;
}

ScopedBrowsingContextGroupPauser::ScopedBrowsingContextGroupPauser(Page& page)
    : browsing_context_group_token_(page.BrowsingContextGroupToken()) {
  CHECK_LT(PausedCount(), std::numeric_limits<uint64_t>::max());
  if (++PausedCount() > 1) {
    return;
  }

  SetPaused(true);
}

ScopedBrowsingContextGroupPauser::~ScopedBrowsingContextGroupPauser() {
  CHECK_GE(PausedCount(), 1u);
  if (--PausedCount() > 0) {
    return;
  }

  SetPaused(false);
}

uint64_t& ScopedBrowsingContextGroupPauser::PausedCount() {
  return PausedCountPerBrowsingContextGroup(browsing_context_group_token_);
}

void ScopedBrowsingContextGroupPauser::SetPaused(bool paused) {
  HeapVector<Member<Page>> pages(Page::OrdinaryPages());
  for (const auto& page : pages) {
    if (page->BrowsingContextGroupToken() == browsing_context_group_token_) {
      page->SetPaused(paused);
    }
  }
}

}  // namespace blink
