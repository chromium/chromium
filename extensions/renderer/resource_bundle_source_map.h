// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_RESOURCE_BUNDLE_SOURCE_MAP_H_
#define EXTENSIONS_RENDERER_RESOURCE_BUNDLE_SOURCE_MAP_H_

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "extensions/renderer/source_map.h"
#include "v8/include/v8-forward.h"

namespace ui {
class ResourceBundle;
}

namespace extensions {

class ResourceBundleSourceMap : public SourceMap {
 public:
  explicit ResourceBundleSourceMap(const ui::ResourceBundle* resource_bundle);

  ResourceBundleSourceMap(const ResourceBundleSourceMap&) = delete;
  ResourceBundleSourceMap& operator=(const ResourceBundleSourceMap&) = delete;

  ~ResourceBundleSourceMap() override;

  v8::Local<v8::String> GetSource(v8::Isolate* isolate,
                                  std::string_view name) const override;
  bool Contains(std::string_view name) const override;

  // Registers `sources` as an additional lookup table. `sources` and the string
  // data behind its keys must outlive `this`; in practice callers pass
  // constexpr tables with static storage duration. Names already present in a
  // previously registered table keep their original resource ID.
  template <size_t N>
  void RegisterSources(const base::fixed_flat_map<std::string_view, int, N>&
                           sources LIFETIME_CAPTURE_BY_THIS) {
    if (sources.empty()) {
      return;
    }
    base::AutoLock lock(lock_);
    source_tables_.emplace_back(sources.begin(), sources.end());
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ResourceBundleSourceMapTest,
                           FirstRegisteredTableWinsForDuplicateNames);
  FRIEND_TEST_ALL_PREFIXES(ResourceBundleSourceMapTest, IgnoresEmptyTables);
  FRIEND_TEST_ALL_PREFIXES(ResourceBundleSourceMapV8Test,
                           LoadsAndCachesGzippedSource);

  using SourceEntry = std::pair<const std::string_view, int>;
  using SourceTable = base::raw_span<const SourceEntry>;

  std::optional<int> FindResourceId(std::string_view name) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  raw_ptr<const ui::ResourceBundle, DanglingUntriaged> resource_bundle_;

  mutable base::Lock lock_;
  std::vector<SourceTable> source_tables_ GUARDED_BY(lock_);
  // The node-based map keeps cached backing strings stable while V8 external
  // strings retain views into them.
  mutable std::map<int, std::string> cached_sources_ GUARDED_BY(lock_);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_RESOURCE_BUNDLE_SOURCE_MAP_H_
