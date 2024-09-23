// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_io_context.h"

#include <array>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/devtools/devtools_stream_blob.h"
#include "content/browser/devtools/devtools_stream_file.h"

namespace content {

DevToolsIOContext::Stream::Stream(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : RefCountedDeleteOnSequence<DevToolsIOContext::Stream>(
          std::move(task_runner)) {}

std::string DevToolsIOContext::Stream::Register(DevToolsIOContext* context) {
  static unsigned s_last_stream_handle = 0;
  const std::string handle = base::NumberToString(++s_last_stream_handle);
  Register(context, handle);
  return handle;
}

void DevToolsIOContext::Stream::Register(DevToolsIOContext* context,
                                         const std::string& handle) {
  context->RegisterStream(this, handle);
}

bool DevToolsIOContext::Stream::SupportsSeek() const {
  return true;
}

DevToolsIOContext::Stream::~Stream() = default;

DevToolsIOContext::DevToolsIOContext() = default;

DevToolsIOContext::~DevToolsIOContext() = default;

void DevToolsIOContext::RegisterStream(scoped_refptr<Stream> stream,
                                       const std::string& id) {
  bool inserted = streams_.emplace(id, std::move(stream)).second;
  DCHECK(inserted);
}

scoped_refptr<DevToolsIOContext::Stream> DevToolsIOContext::GetByHandle(
    const std::string& handle) {
  auto it = streams_.find(handle);
  return it == streams_.end() ? nullptr : it->second;
}

bool DevToolsIOContext::Close(const std::string& handle) {
  size_t erased_count = streams_.erase(handle);
  return !!erased_count;
}

void DevToolsIOContext::DiscardAllStreams() {
  streams_.clear();
}

// static
bool DevToolsIOContext::IsTextMimeType(const std::string& mime_type) {
  static const auto kTextMIMETypePrefixes = std::to_array({
      "text/",
      "application/x-javascript",
      "application/json",
      "application/xml",
  });
  for (size_t i = 0; i < std::size(kTextMIMETypePrefixes); ++i) {
    if (base::StartsWith(mime_type, kTextMIMETypePrefixes[i],
                         base::CompareCase::INSENSITIVE_ASCII))
      return true;
  }
  return false;
}

}  // namespace content
