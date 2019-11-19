// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_error_handler.h"

#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/url_request/url_request.h"

using net::SSLInfo;

namespace content {

SSLErrorHandler::SSLErrorHandler(WebContents* web_contents,
                                 const base::WeakPtr<Delegate>& delegate,
                                 bool is_main_frame_request,
                                 const GURL& url,
                                 int net_error,
                                 const net::SSLInfo& ssl_info,
                                 bool fatal)
    : delegate_(delegate),
      request_url_(url),
      is_main_frame_request_(is_main_frame_request),
      ssl_info_(ssl_info),
      cert_error_(net_error),
      fatal_(fatal),
      web_contents_(web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

SSLErrorHandler::~SSLErrorHandler() {}

void SSLErrorHandler::CancelRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (delegate_)
    delegate_->CancelSSLRequest(net::ERR_ABORTED, &ssl_info());
}

void SSLErrorHandler::DenyRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (delegate_)
    delegate_->CancelSSLRequest(cert_error_, &ssl_info());
}

void SSLErrorHandler::ContinueRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (delegate_)
    delegate_->ContinueSSLRequest();
}

}  // namespace content
