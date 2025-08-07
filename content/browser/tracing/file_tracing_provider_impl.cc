// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/file_tracing_provider_impl.h"

#include "base/files/file_path.h"
#include "base/trace_event/trace_event.h"

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
  TRACE_EVENT_BEGIN(kFileTracingEventCategoryGroup, FILE_TRACING_PREFIX,
                    perfetto::Track::FromPointer(id));
}

void FileTracingProviderImpl::FileTracingDisable(const void* id) {
  // FILE_TRACING_PREFIX
  TRACE_EVENT_END(kFileTracingEventCategoryGroup,
                  perfetto::Track::FromPointer(id));
}

void FileTracingProviderImpl::FileTracingEventBegin(const char* name,
                                                    const void* id,
                                                    const base::FilePath& path,
                                                    int64_t size) {
  TRACE_EVENT_BEGIN(kFileTracingEventCategoryGroup,
                    perfetto::DynamicString(name),
                    perfetto::Track::FromPointer(id), "path",
                    path.AsUTF8Unsafe(), "size", size);
}

void FileTracingProviderImpl::FileTracingEventEnd(const char* name,
                                                  const void* id) {
  TRACE_EVENT_END(kFileTracingEventCategoryGroup,
                  perfetto::Track::FromPointer(id));
}

}  // namespace content
