// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_STREAM_FILE_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_STREAM_FILE_H_

#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/devtools/devtools_io_context.h"

#include <string>

namespace content {

class DevToolsStreamFile : public DevToolsIOContext::Stream {
 public:
  static scoped_refptr<DevToolsStreamFile> Create(DevToolsIOContext* context,
                                                  bool binary);
  const std::string& handle() const { return handle_; }
  void Append(std::unique_ptr<std::string> data);

 private:
  DevToolsStreamFile(DevToolsIOContext* context, bool binary);
  ~DevToolsStreamFile() override;

  void Read(off_t position, size_t max_size, ReadCallback callback) override;

  void ReadOnFileSequence(off_t position,
                          size_t max_size,
                          ReadCallback callback);
  Status InnerReadOnFileSequence(off_t position,
                                 size_t max_size,
                                 std::string& out_data);
  void AppendOnFileSequence(std::unique_ptr<std::string> data);
  bool InitOnFileSequenceIfNeeded();

  const std::string handle_;
  const bool binary_;

  base::File file_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  bool had_errors_ = false;
  off_t last_written_pos_ = 0;
  off_t last_read_pos_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_STREAM_FILE_H_
