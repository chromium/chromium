// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/string_source_map.h"

#include "base/containers/contains.h"
#include "gin/converter.h"
#include "third_party/zlib/google/compression_utils.h"

namespace extensions {

StringSourceMap::StringSourceMap() = default;
StringSourceMap::~StringSourceMap() = default;

v8::Local<v8::String> StringSourceMap::GetSource(
    v8::Isolate* isolate,
    const std::string& name) const {
  const auto& iter = sources_.find(name);
  if (iter == sources_.end())
    return v8::Local<v8::String>();
  return gin::StringToV8(isolate, iter->second);
}

bool StringSourceMap::Contains(const std::string& name) const {
  return base::Contains(sources_, name);
}

void StringSourceMap::RegisterModule(const std::string& name,
                                     const std::string& source,
                                     bool gzipped) {
  CHECK_EQ(0u, sources_.count(name)) << "A module for '" << name
                                     << "' already exists.";
  if (!gzipped) {
    sources_[name] = source;
    return;
  }

  std::string uncompressed;
  CHECK(compression::GzipUncompress(source, &uncompressed));
  sources_[name] = uncompressed;
}

}  // namespace extensions
