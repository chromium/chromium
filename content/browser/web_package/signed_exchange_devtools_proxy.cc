// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_devtools_proxy.h"

#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

SignedExchangeDevToolsProxy::SignedExchangeDevToolsProxy(
    const GURL& outer_request_url,
    network::mojom::URLResponseHeadPtr outer_response,
    FrameTreeNodeId frame_tree_node_id,
    std::optional<const base::UnguessableToken> devtools_navigation_token,
    bool report_raw_headers)
    : outer_request_url_(outer_request_url),
      outer_response_(std::move(outer_response)),
      frame_tree_node_id_(frame_tree_node_id),
      devtools_navigation_token_(devtools_navigation_token),
      devtools_enabled_(report_raw_headers) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

SignedExchangeDevToolsProxy::~SignedExchangeDevToolsProxy() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void SignedExchangeDevToolsProxy::ReportError(
    const std::string& message,
    std::optional<SignedExchangeError::FieldIndexPair> error_field) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  errors_.push_back(SignedExchangeError(message, std::move(error_field)));
  WebContents* web_contents =
      WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  if (!web_contents)
    return;
  web_contents->GetPrimaryMainFrame()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError, message);
}

void SignedExchangeDevToolsProxy::CertificateRequestSent(
    const base::UnguessableToken& request_id,
    const network::ResourceRequest& request) {
  if (!devtools_enabled_)
    return;

  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  if (!frame_tree_node)
    return;

  devtools_instrumentation::OnSignedExchangeCertificateRequestSent(
      frame_tree_node, request_id,
      devtools_navigation_token_ ? *devtools_navigation_token_ : request_id,
      request, outer_request_url_);
}

void SignedExchangeDevToolsProxy::CertificateResponseReceived(
    const base::UnguessableToken& request_id,
    const GURL& url,
    const network::mojom::URLResponseHead& head) {
  if (!devtools_enabled_)
    return;

  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  if (!frame_tree_node)
    return;

  devtools_instrumentation::OnSignedExchangeCertificateResponseReceived(
      frame_tree_node, request_id,
      devtools_navigation_token_ ? *devtools_navigation_token_ : request_id,
      url, head);
}

void SignedExchangeDevToolsProxy::CertificateRequestCompleted(
    const base::UnguessableToken& request_id,
    const network::URLLoaderCompletionStatus& status) {
  if (!devtools_enabled_)
    return;

  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  if (!frame_tree_node)
    return;

  devtools_instrumentation::OnSignedExchangeCertificateRequestCompleted(
      frame_tree_node, request_id, status);
}

void SignedExchangeDevToolsProxy::OnSignedExchangeReceived(
    const std::optional<SignedExchangeEnvelope>& envelope,
    const scoped_refptr<net::X509Certificate>& certificate,
    const std::optional<net::SSLInfo>& ssl_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!devtools_enabled_)
    return;

  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  if (!frame_tree_node)
    return;

  devtools_instrumentation::OnSignedExchangeReceived(
      frame_tree_node, devtools_navigation_token_, outer_request_url_,
      *outer_response_, envelope, certificate, ssl_info, std::move(errors_));
}

}  // namespace content
