// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_error_handler.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/url_request/url_request.h"

using net::SSLInfo;

namespace content {

namespace {

void CompleteCancelRequest(
    const base::WeakPtr<SSLErrorHandler::Delegate>& delegate,
    const net::SSLInfo& ssl_info,
    int error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (delegate.get())
    delegate->CancelSSLRequest(error, &ssl_info);
}

void CompleteContinueRequest(
    const base::WeakPtr<SSLErrorHandler::Delegate>& delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (delegate.get()) {
    delegate->ContinueSSLRequest();
  }
}

}  // namespace

SSLErrorHandler::SSLErrorHandler(WebContents* web_contents,
                                 const base::WeakPtr<Delegate>& delegate,
                                 BrowserThread::ID delegate_thread,
                                 ResourceType resource_type,
                                 const GURL& url,
                                 const net::SSLInfo& ssl_info,
                                 bool fatal)
    : delegate_(delegate),
      delegate_thread_(delegate_thread),
      request_url_(url),
      resource_type_(resource_type),
      ssl_info_(ssl_info),
      cert_error_(net::MapCertStatusToNetError(ssl_info.cert_status)),
      fatal_(fatal),
      web_contents_(web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(delegate_thread == BrowserThread::UI ||
         delegate_thread == BrowserThread::IO);
}

SSLErrorHandler::~SSLErrorHandler() {}

void SSLErrorHandler::CancelRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (delegate_thread_ == BrowserThread::UI) {
    if (delegate_)
      delegate_->CancelSSLRequest(net::ERR_ABORTED, &ssl_info());
    return;
  }
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(&CompleteCancelRequest, delegate_,
                                          ssl_info(), net::ERR_ABORTED));
}

void SSLErrorHandler::DenyRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (delegate_thread_ == BrowserThread::UI) {
    if (delegate_)
      delegate_->CancelSSLRequest(cert_error_, &ssl_info());
    return;
  }
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(&CompleteCancelRequest, delegate_,
                                          ssl_info(), cert_error_));
}

void SSLErrorHandler::ContinueRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (delegate_thread_ == BrowserThread::UI) {
    if (delegate_)
      delegate_->ContinueSSLRequest();
    return;
  }
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(&CompleteContinueRequest, delegate_));
}

}  // namespace content
