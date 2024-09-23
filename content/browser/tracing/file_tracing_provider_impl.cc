// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/file_tracing_provider_impl.h"

#include "base/files/file_path.h"
#include "base/trace_event/base_tracing.h"

namespace content {

constexpr const char kFileTracingEventCategoryGroup[] =
    TRACE_DISABLED_BY_DEFAULT("file");

FileTracingProviderImpl::FileTracingProviderImpl() {}
FileTracingProviderImpl::~FileTracingProviderImpl() {}

bool FileTracingProviderImpl::FileTracingCategoryIsEnabled() const {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kFileTracingEventCategoryGroup, &enabled);
  return enabled;
}

void FileTracingProviderImpl::FileTracingEnable(const void* id) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      kFileTracingEventCategoryGroup, FILE_TRACING_PREFIX, id);
}

void FileTracingProviderImpl::FileTracingDisable(const void* id) {
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      kFileTracingEventCategoryGroup, FILE_TRACING_PREFIX, id);
}

void FileTracingProviderImpl::FileTracingEventBegin(const char* name,
                                                    const void* id,
                                                    const base::FilePath& path,
                                                    int64_t size) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(kFileTracingEventCategoryGroup, name, id,
      "path", path.AsUTF8Unsafe(), "size", size);
}

void FileTracingProviderImpl::FileTracingEventEnd(const char* name,
                                                  const void* id) {
  TRACE_EVENT_NESTABLE_ASYNC_END0(kFileTracingEventCategoryGroup, name, id);
}

}  // namespace content
