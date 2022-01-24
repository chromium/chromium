// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_enumeration_cache.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/pass_key.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"

namespace content {

// static
base::SequenceBound<FontEnumerationCache> FontEnumerationCache::Create() {
  return FontEnumerationCache::CreateForTesting(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      absl::nullopt);
}

#if !defined(PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL)

// static
base::SequenceBound<FontEnumerationCache>
FontEnumerationCache::CreateForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    absl::optional<std::string> locale_override) {
  return base::SequenceBound<FontEnumerationCache>();
}

#endif  // !defined(PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL)

FontEnumerationCache::FontEnumerationCache(
    absl::optional<std::string> locale_override)
    : locale_override_(std::move(locale_override)) {}

FontEnumerationCache::~FontEnumerationCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

FontEnumerationData FontEnumerationCache::GetFontEnumerationData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!initialized_) {
    std::string locale =
        locale_override_.value_or(base::i18n::GetConfiguredLocale());

    blink::FontEnumerationTable font_data = ComputeFontEnumerationData(locale);
    BuildEnumerationCache(font_data);
    initialized_ = true;
  }

  DCHECK(initialized_);
  return {.status = data_.status, .font_data = data_.font_data.Duplicate()};
}

void FontEnumerationCache::BuildEnumerationCache(
    blink::FontEnumerationTable& table) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Postscript names, according to spec, are expected to be encoded in a subset
  // of ASCII. See:
  // https://docs.microsoft.com/en-us/typography/opentype/spec/name This is why
  // a "simple" byte-wise comparison is used.
  std::sort(table.mutable_fonts()->begin(), table.mutable_fonts()->end(),
            [](const blink::FontEnumerationTable_FontMetadata& a,
               const blink::FontEnumerationTable_FontMetadata& b) {
              return a.postscript_name() < b.postscript_name();
            });

  base::MappedReadOnlyRegion font_data_region =
      base::ReadOnlySharedMemoryRegion::Create(table.ByteSizeLong());

  if (!font_data_region.mapping.IsValid()) {
    data_.status = blink::mojom::FontEnumerationStatus::kUnexpectedError;
    return;
  }

  DCHECK_GE(font_data_region.mapping.size(), table.ByteSizeLong());
  if (!table.SerializeToArray(font_data_region.mapping.memory(),
                              font_data_region.mapping.size())) {
    data_.status = blink::mojom::FontEnumerationStatus::kUnexpectedError;
    return;
  }

  data_.status = blink::mojom::FontEnumerationStatus::kOk;
  data_.font_data = std::move(font_data_region.region);
  return;
}

}  // namespace content
