// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/devtools_mhtml_helper.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/mhtml_generation_params.h"
#include "storage/browser/blob/shareable_file_reference.h"

namespace content {
namespace protocol {

namespace {

void ClearFileReferenceOnIOThread(
    scoped_refptr<storage::ShareableFileReference>) {}

}  // namespace

DevToolsMHTMLHelper::DevToolsMHTMLHelper(
    base::WeakPtr<PageHandler> page_handler,
    std::unique_ptr<PageHandler::CaptureSnapshotCallback> callback)
    : page_handler_(page_handler), callback_(std::move(callback)) {}

DevToolsMHTMLHelper::~DevToolsMHTMLHelper() {
  if (mhtml_file_.get()) {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&ClearFileReferenceOnIOThread, std::move(mhtml_file_)));
  }
}

// static
void DevToolsMHTMLHelper::Capture(
    base::WeakPtr<PageHandler> page_handler,
    std::unique_ptr<PageHandler::CaptureSnapshotCallback> callback) {
  scoped_refptr<DevToolsMHTMLHelper> helper =
      new DevToolsMHTMLHelper(page_handler, std::move(callback));
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(),
       // Requires IO.
       base::MayBlock(),

       // TaskShutdownBehavior: use SKIP_ON_SHUTDOWN so that the helper's
       // fields do not suddenly become invalid.
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&DevToolsMHTMLHelper::CreateTemporaryFile, helper));
}

void DevToolsMHTMLHelper::CreateTemporaryFile() {
  if (!base::CreateTemporaryFile(&mhtml_snapshot_path_)) {
    ReportFailure("Unable to create temporary file");
    return;
  }
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&DevToolsMHTMLHelper::TemporaryFileCreatedOnIO, this));
}

void DevToolsMHTMLHelper::TemporaryFileCreatedOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Setup a ShareableFileReference so the temporary file gets deleted
  // once it is no longer used.
  mhtml_file_ = storage::ShareableFileReference::GetOrCreate(
      mhtml_snapshot_path_,
      storage::ShareableFileReference::DELETE_ON_FINAL_RELEASE,
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(),
           // Requires IO.
           base::MayBlock(),

           // Because we are using DELETE_ON_FINAL_RELEASE here, the
           // storage::ScopedFile inside ShareableFileReference requires
           // a shutdown blocking task runner to ensure that its deletion
           // task runs.
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})
          .get());

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&DevToolsMHTMLHelper::TemporaryFileCreatedOnUI, this));
}

void DevToolsMHTMLHelper::TemporaryFileCreatedOnUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!page_handler_) {
    ReportFailure("");
    return;
  }
  WebContentsImpl* web_contents = page_handler_->GetWebContents();
  if (!web_contents) {
    ReportFailure("No web contents");
    return;
  }
  web_contents->GenerateMHTML(
      content::MHTMLGenerationParams(mhtml_snapshot_path_),
      base::BindOnce(&DevToolsMHTMLHelper::MHTMLGeneratedOnUI, this));
}

void DevToolsMHTMLHelper::MHTMLGeneratedOnUI(int64_t mhtml_file_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (mhtml_file_size <= 0 ||
      mhtml_file_size > std::numeric_limits<int>::max()) {
    ReportFailure("Failed to generate MHTML");
    return;
  }
  base::PostTask(FROM_HERE,
                 {base::ThreadPool(),
                  // Requires IO.
                  base::MayBlock(),

                  // TaskShutdownBehavior: use SKIP_ON_SHUTDOWN so that the
                  // helper's fields do not suddenly become invalid.
                  base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
                 base::BindOnce(&DevToolsMHTMLHelper::ReadMHTML, this));
}

void DevToolsMHTMLHelper::ReadMHTML() {
  std::string buffer;
  if (!base::ReadFileToString(mhtml_snapshot_path_, &buffer)) {
    LOG(ERROR) << "Failed to read " << mhtml_snapshot_path_;
    ReportFailure("Failed to read MHTML file");
    return;
  }
  std::unique_ptr<std::string> buffer_ptr =
      std::make_unique<std::string>(buffer);
  ReportSuccess(std::move(buffer_ptr));
}

void DevToolsMHTMLHelper::ReportFailure(const std::string& message) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&DevToolsMHTMLHelper::ReportFailure, this, message));
    return;
  }
  if (message.empty())
    callback_->sendFailure(Response::InternalError());
  else
    callback_->sendFailure(Response::Error(message));
}

void DevToolsMHTMLHelper::ReportSuccess(
    std::unique_ptr<std::string> mhtml_snapshot) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&DevToolsMHTMLHelper::ReportSuccess, this,
                                  std::move(mhtml_snapshot)));
    return;
  }
  callback_->sendSuccess(*mhtml_snapshot);
}

}  // namespace protocol
}  // namespace content
