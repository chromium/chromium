// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_global_context.h"

#include "base/memory/ptr_util.h"
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
    : memory_pressure_listener_registration_(
          FROM_HERE,
          base::MemoryPressureListenerTag::kFontGlobalContext,
          this) {}

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

void FontGlobalContext::OnMemoryPressure(
    base::MemoryPressureLevel memory_pressure_level) {
  if (memory_pressure_level == base::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  font_cache_.Invalidate();
}

}  // namespace blink
