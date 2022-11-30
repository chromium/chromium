// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_FILE_TRACING_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_TRACING_FILE_TRACING_PROVIDER_IMPL_H_

#include <stdint.h>

#include "base/files/file_tracing.h"

namespace content {

class FileTracingProviderImpl : public base::FileTracing::Provider {
 public:
  FileTracingProviderImpl();

  FileTracingProviderImpl(const FileTracingProviderImpl&) = delete;
  FileTracingProviderImpl& operator=(const FileTracingProviderImpl&) = delete;

  ~FileTracingProviderImpl() override;

  // base::FileTracing::Provider:
  bool FileTracingCategoryIsEnabled() const override;
  void FileTracingEnable(const void* id) override;
  void FileTracingDisable(const void* id) override;
  void FileTracingEventBegin(const char* name,
                             const void* id,
                             const base::FilePath& path,
                             int64_t size) override;
  void FileTracingEventEnd(const char* name, const void* id) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_FILE_TRACING_PROVIDER_IMPL_H_
