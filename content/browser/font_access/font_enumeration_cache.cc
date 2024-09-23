// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_enumeration_cache.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "content/browser/font_access/font_enumeration_data_source.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom.h"

namespace content {

// static
base::SequenceBound<FontEnumerationCache> FontEnumerationCache::Create() {
  return base::SequenceBound<FontEnumerationCache>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      FontEnumerationDataSource::Create(), std::nullopt,
      base::PassKey<FontEnumerationCache>());
}

// static
base::SequenceBound<FontEnumerationCache>
FontEnumerationCache::CreateForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<FontEnumerationDataSource> data_source,
    std::optional<std::string> locale_override) {
  DCHECK(data_source);
  return base::SequenceBound<FontEnumerationCache>(
      std::move(task_runner), std::move(data_source),
      std::move(locale_override), base::PassKey<FontEnumerationCache>());
}

FontEnumerationCache::FontEnumerationCache(
    std::unique_ptr<FontEnumerationDataSource> data_source,
    std::optional<std::string> locale_override,
    base::PassKey<FontEnumerationCache>)
    : data_source_(std::move(data_source)),
      locale_override_(std::move(locale_override)) {
  DCHECK(data_source_);
}

FontEnumerationCache::~FontEnumerationCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

FontEnumerationData FontEnumerationCache::GetFontEnumerationData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!initialized_) {
    std::string locale =
        locale_override_.value_or(base::i18n::GetConfiguredLocale());

    blink::FontEnumerationTable font_data = data_source_->GetFonts(locale);
    BuildEnumerationCache(font_data);
    initialized_ = true;
  }

  DCHECK(initialized_);
  return {.status = data_.status, .font_data = data_.font_data.Duplicate()};
}

void FontEnumerationCache::BuildEnumerationCache(
    blink::FontEnumerationTable& table) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!FontEnumerationDataSource::IsOsSupported()) {
    data_.status = blink::mojom::FontEnumerationStatus::kUnimplemented;
    return;
  }

  // Postscript names, according to spec, are expected to be encoded in a subset
  // of ASCII. See:
  // https://docs.microsoft.com/en-us/typography/opentype/spec/name This is why
  // a "simple" byte-wise comparison is used.
  std::sort(table.mutable_fonts()->begin(), table.mutable_fonts()->end(),
            [](const blink::FontEnumerationTable_FontData& a,
               const blink::FontEnumerationTable_FontData& b) {
              return a.postscript_name() < b.postscript_name();
            });

  base::MappedReadOnlyRegion font_data_region =
      base::ReadOnlySharedMemoryRegion::Create(table.ByteSizeLong());

  if (!font_data_region.mapping.IsValid()) {
    data_.status = blink::mojom::FontEnumerationStatus::kUnexpectedError;
    return;
  }

  DCHECK_GE(font_data_region.mapping.size(), table.ByteSizeLong());
  base::span<uint8_t> font_mem(font_data_region.mapping);
  if (!table.SerializeToArray(font_mem.data(), font_mem.size())) {
    data_.status = blink::mojom::FontEnumerationStatus::kUnexpectedError;
    return;
  }

  data_.status = blink::mojom::FontEnumerationStatus::kOk;
  data_.font_data = std::move(font_data_region.region);
  return;
}

}  // namespace content
