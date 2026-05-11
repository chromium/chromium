// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_global_context.h"

#include "base/memory/ptr_util.h"
#include "base/memory_coordinator/utils.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

ThreadSpecific<Persistent<FontGlobalContext>>&
GetThreadSpecificFontGlobalContextPool() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<Persistent<FontGlobalContext>>,
                                  thread_specific_pool, ());
  return thread_specific_pool;
}

FontGlobalContext& FontGlobalContext::Get() {
  auto& thread_specific_pool = GetThreadSpecificFontGlobalContextPool();
  if (!*thread_specific_pool)
    *thread_specific_pool = MakeGarbageCollected<FontGlobalContext>(PassKey());
  return **thread_specific_pool;
}

FontGlobalContext* FontGlobalContext::TryGet() {
  return GetThreadSpecificFontGlobalContextPool()->Get();
}

FontGlobalContext::FontGlobalContext(PassKey)
    : memory_consumer_registration_(
          "FontGlobalContext",
          /*traits=*/std::nullopt,  // TODO(crbug.com/489671163): Fill traits.
          this,
          MemoryConsumerRegistration::CheckUnregister::kDisabled,
          MemoryConsumerRegistration::CheckRegistryExists::kDisabled) {}

void FontGlobalContext::Dispose() {
  memory_consumer_registration_.Dispose();
}

FontGlobalContext::~FontGlobalContext() = default;

FontUniqueNameLookup* FontGlobalContext::GetFontUniqueNameLookup() {
  if (!Get().font_unique_name_lookup_) {
    Get().font_unique_name_lookup_ =
        FontUniqueNameLookup::GetPlatformUniqueNameLookup();
  }
  return Get().font_unique_name_lookup_.get();
}

void FontGlobalContext::Init() {
  DCHECK(IsMainThread());
  if (auto* name_lookup = FontGlobalContext::Get().GetFontUniqueNameLookup())
    name_lookup->Init();
  HarfBuzzFace::Init();
}

void FontGlobalContext::OnUpdateMemoryLimit() {}

void FontGlobalContext::OnReleaseMemory() {
  if (memory_limit() <= base::kModerateMemoryPressureThreshold) {
    font_cache_.Invalidate();
  }
}

}  // namespace blink
