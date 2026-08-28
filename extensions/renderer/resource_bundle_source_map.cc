// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/resource_bundle_source_map.h"

#include <algorithm>
#include <functional>
#include <ostream>
#include <string_view>

#include "base/notreached.h"
#include "extensions/renderer/static_v8_external_one_byte_string_resource.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

namespace {

v8::Local<v8::String> ConvertString(v8::Isolate* isolate,
                                    std::string_view string) {
  // v8 takes ownership of the StaticV8ExternalOneByteStringResource (see
  // v8::String::NewExternalOneByte()).
  return v8::String::NewExternalOneByte(
             isolate, new StaticV8ExternalOneByteStringResource(string))
      .FromMaybe(v8::Local<v8::String>());
}

}  // namespace

ResourceBundleSourceMap::ResourceBundleSourceMap(
    const ui::ResourceBundle* resource_bundle)
    : resource_bundle_(resource_bundle) {}

ResourceBundleSourceMap::~ResourceBundleSourceMap() = default;

v8::Local<v8::String> ResourceBundleSourceMap::GetSource(
    v8::Isolate* isolate,
    std::string_view name) const {
  base::AutoLock lock(lock_);
  std::optional<int> resource_id = FindResourceId(name);
  if (!resource_id) {
    DUMP_WILL_BE_NOTREACHED()
        << "No module is registered with name \"" << name << "\"";
    return v8::Local<v8::String>();
  }

  auto cached = cached_sources_.find(*resource_id);
  if (cached != cached_sources_.end()) {
    return ConvertString(isolate, cached->second);
  }

  std::string_view resource =
      resource_bundle_->GetRawDataResource(*resource_id);
  if (resource.empty()) {
    DUMP_WILL_BE_NOTREACHED()
        << "Module resource registered as \"" << name << "\" not found";
    return v8::Local<v8::String>();
  }

  bool is_gzipped = resource_bundle_->IsGzipped(*resource_id);
  if (is_gzipped) {
    std::string& cached_source = cached_sources_[*resource_id];
    if (!compression::GzipUncompress(resource, &cached_source)) {
      // Leave an empty string in the cache so later requests return directly
      // instead of trying to uncompress again.
      cached_source.clear();
      return v8::Local<v8::String>();
    }
    resource = cached_source;
  }

  return ConvertString(isolate, resource);
}

bool ResourceBundleSourceMap::Contains(std::string_view name) const {
  base::AutoLock lock(lock_);
  return FindResourceId(name).has_value();
}

std::optional<int> ResourceBundleSourceMap::FindResourceId(
    std::string_view name) const {
  for (const SourceTable& table : source_tables_) {
    auto source = std::ranges::lower_bound(table, name, std::less<>(),
                                           &SourceEntry::first);
    if (source != table.end() && source->first == name) {
      return source->second;
    }
  }
  return std::nullopt;
}

}  // namespace extensions
