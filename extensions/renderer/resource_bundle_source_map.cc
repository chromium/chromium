// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/resource_bundle_source_map.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "extensions/renderer/static_v8_external_one_byte_string_resource.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {

namespace {

v8::Local<v8::String> ConvertString(v8::Isolate* isolate,
                                    const base::StringPiece& string) {
  // v8 takes ownership of the StaticV8ExternalOneByteStringResource (see
  // v8::String::NewExternalOneByte()).
  return v8::String::NewExternalOneByte(
             isolate, new StaticV8ExternalOneByteStringResource(string))
      .FromMaybe(v8::Local<v8::String>());
}

}  // namespace

ResourceBundleSourceMap::ResourceInfo::ResourceInfo() = default;
ResourceBundleSourceMap::ResourceInfo::ResourceInfo(int in_id) : id(in_id) {}
ResourceBundleSourceMap::ResourceInfo::ResourceInfo(ResourceInfo&& other) =
    default;

ResourceBundleSourceMap::ResourceInfo::~ResourceInfo() = default;

ResourceBundleSourceMap::ResourceInfo& ResourceBundleSourceMap::ResourceInfo::
operator=(ResourceInfo&& other) = default;

ResourceBundleSourceMap::ResourceBundleSourceMap(
    const ui::ResourceBundle* resource_bundle)
    : resource_bundle_(resource_bundle) {
}

ResourceBundleSourceMap::~ResourceBundleSourceMap() {
}

void ResourceBundleSourceMap::RegisterSource(const char* const name,
                                             int resource_id) {
  resource_map_.emplace(name, resource_id);
}

v8::Local<v8::String> ResourceBundleSourceMap::GetSource(
    v8::Isolate* isolate,
    const std::string& name) const {
  auto resource_iter = resource_map_.find(name);
  if (resource_iter == resource_map_.end()) {
    NOTREACHED() << "No module is registered with name \"" << name << "\"";
    return v8::Local<v8::String>();
  }

  const ResourceInfo& info = resource_iter->second;
  if (info.cached)
    return ConvertString(isolate, *info.cached);

  base::StringPiece resource = resource_bundle_->GetRawDataResource(info.id);
  if (resource.empty()) {
    NOTREACHED()
        << "Module resource registered as \"" << name << "\" not found";
    return v8::Local<v8::String>();
  }

  bool is_gzipped = resource_bundle_->IsGzipped(info.id);
  if (is_gzipped) {
    info.cached = std::make_unique<std::string>();
    uint32_t size = compression::GetUncompressedSize(resource);
    info.cached->resize(size);
    base::StringPiece uncompressed(*info.cached);
    if (!compression::GzipUncompress(resource, uncompressed)) {
      // Let |info.cached| point to an empty string, so that the next time when
      // the resource is requested, the method returns an empty string directly,
      // instead of trying to uncompress again.
      info.cached->clear();
      return v8::Local<v8::String>();
    }
    resource = uncompressed;
  }

  return ConvertString(isolate, resource);
}

bool ResourceBundleSourceMap::Contains(const std::string& name) const {
  return base::Contains(resource_map_, name);
}

}  // namespace extensions
