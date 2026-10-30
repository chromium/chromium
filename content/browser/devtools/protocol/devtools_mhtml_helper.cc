// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/devtools_mhtml_helper.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/mhtml_generation_params.h"

namespace content {
namespace protocol {

DevToolsMHTMLHelper::DevToolsMHTMLHelper(
    const WebContents::Getter& web_contents_getter,
    std::unique_ptr<PageHandler::CaptureSnapshotCallback> callback)
    : web_contents_getter_(web_contents_getter),
      callback_(std::move(callback)),
      file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}

DevToolsMHTMLHelper::~DevToolsMHTMLHelper() {
  if (!mhtml_path_.empty()) {
    file_task_runner_->PostTask(FROM_HERE,
                                base::GetDeleteFileCallback(mhtml_path_));
  }
}

// static
void DevToolsMHTMLHelper::Capture(
    const WebContents::Getter& web_contents_getter,
    std::unique_ptr<PageHandler::CaptureSnapshotCallback> callback) {
  scoped_refptr<DevToolsMHTMLHelper> helper =
      new DevToolsMHTMLHelper(web_contents_getter, std::move(callback));
  helper->file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DevToolsMHTMLHelper::CreateTemporaryFile),
      base::BindOnce(&DevToolsMHTMLHelper::TemporaryFileCreated, helper));
}

// static
base::FilePath DevToolsMHTMLHelper::CreateTemporaryFile() {
  base::FilePath path;
  if (!base::CreateTemporaryFile(&path)) {
    return {};
  }
  return path;
}

void DevToolsMHTMLHelper::TemporaryFileCreated(base::FilePath path) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  mhtml_path_ = std::move(path);
  if (mhtml_path_.empty()) {
    ReportFailure("Unable to create temporary file");
    return;
  }
  WebContents* web_contents = web_contents_getter_.Run();
  if (!web_contents) {
    ReportFailure("No web contents");
    return;
  }
  web_contents->GenerateMHTML(
      content::MHTMLGenerationParams(mhtml_path_),
      base::BindOnce(&DevToolsMHTMLHelper::MHTMLGeneratedOnUI, this));
}

void DevToolsMHTMLHelper::MHTMLGeneratedOnUI(int64_t mhtml_file_size) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (mhtml_file_size <= 0 ||
      mhtml_file_size > std::numeric_limits<int>::max()) {
    ReportFailure("Failed to generate MHTML");
    return;
  }
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DevToolsMHTMLHelper::ReadMHTML, mhtml_path_),
      base::BindOnce(&DevToolsMHTMLHelper::MHTMLRead, this));
}

// static
std::optional<std::string> DevToolsMHTMLHelper::ReadMHTML(
    const base::FilePath& path) {
  std::string buffer;
  if (!base::ReadFileToString(path, &buffer)) {
    LOG(ERROR) << "Failed to read " << path;
    return std::nullopt;
  }
  return buffer;
}

void DevToolsMHTMLHelper::ReportFailure(const std::string& message) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (message.empty()) {
    callback_->sendFailure(Response::InternalError());
  } else {
    callback_->sendFailure(Response::ServerError(message));
  }
}

void DevToolsMHTMLHelper::MHTMLRead(std::optional<std::string> mhtml_snapshot) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (!mhtml_snapshot) {
    ReportFailure("Failed to read MHTML file");
    return;
  }
  callback_->sendSuccess(*mhtml_snapshot);
}

}  // namespace protocol
}  // namespace content
