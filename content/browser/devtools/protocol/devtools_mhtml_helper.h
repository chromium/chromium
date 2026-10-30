// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_MHTML_HELPER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_MHTML_HELPER_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/browser/devtools/protocol/page_handler.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {
namespace protocol {

class DevToolsMHTMLHelper : public base::RefCounted<DevToolsMHTMLHelper> {
 public:
  static void Capture(
      const WebContents::Getter& web_contents_getter,
      std::unique_ptr<PageHandler::CaptureSnapshotCallback> callback);

 private:
  DevToolsMHTMLHelper(
      const WebContents::Getter& web_contents_getter,
      std::unique_ptr<PageHandler::CaptureSnapshotCallback> callback);
  ~DevToolsMHTMLHelper();

  static base::FilePath CreateTemporaryFile();
  void TemporaryFileCreated(base::FilePath path);
  void MHTMLGeneratedOnUI(int64_t mhtml_file_size);
  static std::optional<std::string> ReadMHTML(const base::FilePath& path);
  void ReportFailure(const std::string& message);
  void MHTMLRead(std::optional<std::string> mhtml_snapshot);

  WebContents::Getter web_contents_getter_;
  std::unique_ptr<PageHandler::CaptureSnapshotCallback> callback_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  base::FilePath mhtml_path_;

  friend class base::RefCounted<DevToolsMHTMLHelper>;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_MHTML_HELPER_H_
