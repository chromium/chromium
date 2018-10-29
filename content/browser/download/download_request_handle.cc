// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_request_handle.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

DownloadRequestHandle::DownloadRequestHandle(
    const DownloadRequestHandle& other) = default;

DownloadRequestHandle::~DownloadRequestHandle() {}

DownloadRequestHandle::DownloadRequestHandle() {}

DownloadRequestHandle::DownloadRequestHandle(
    const base::WeakPtr<DownloadResourceHandler>& handler,
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter)
    : handler_(handler), web_contents_getter_(web_contents_getter) {
  DCHECK(handler_.get());
}

WebContents* DownloadRequestHandle::GetWebContents() const {
  return web_contents_getter_.is_null() ? nullptr : web_contents_getter_.Run();
}

void DownloadRequestHandle::PauseRequest() {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&DownloadResourceHandler::PauseRequest, handler_));
}

void DownloadRequestHandle::ResumeRequest() {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&DownloadResourceHandler::ResumeRequest, handler_));
}

void DownloadRequestHandle::CancelRequest(bool user_cancel) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&DownloadResourceHandler::CancelRequest, handler_));
}

}  // namespace content
