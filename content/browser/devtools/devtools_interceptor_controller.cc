// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_interceptor_controller.h"

#include "base/supports_user_data.h"
#include "base/task/post_task.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {
const char kDevToolsInterceptorController[] = "DevToolsInterceptorController";
}

std::unique_ptr<InterceptionHandle>
DevToolsInterceptorController::StartInterceptingRequests(
    const FrameTreeNode* target_frame,
    std::vector<Pattern> intercepted_patterns,
    RequestInterceptedCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const base::UnguessableToken& target_id =
      target_frame->devtools_frame_token();

  auto filter_entry = std::make_unique<DevToolsNetworkInterceptor::FilterEntry>(
      target_id, std::move(intercepted_patterns), std::move(callback));
  DevToolsTargetRegistry::RegistrationHandle registration_handle =
      target_registry_->RegisterWebContents(
          WebContentsImpl::FromFrameTreeNode(target_frame));
  std::unique_ptr<InterceptionHandle> handle(new InterceptionHandle(
      std::move(registration_handle), interceptor_, filter_entry.get()));

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&DevToolsNetworkInterceptor::AddFilterEntry, interceptor_,
                     std::move(filter_entry)));
  return handle;
}

void DevToolsInterceptorController::ContinueInterceptedRequest(
    std::string interception_id,
    std::unique_ptr<Modifications> modifications,
    std::unique_ptr<ContinueInterceptedRequestCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&DevToolsNetworkInterceptor::ContinueInterceptedRequest,
                     interceptor_, interception_id, std::move(modifications),
                     std::move(callback)));
}

bool DevToolsInterceptorController::ShouldCancelNavigation(
    const GlobalRequestID& global_request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = canceled_navigation_requests_.find(global_request_id);
  if (it == canceled_navigation_requests_.end())
    return false;
  canceled_navigation_requests_.erase(it);
  return true;
}

void DevToolsInterceptorController::GetResponseBody(
    std::string interception_id,
    std::unique_ptr<GetResponseBodyForInterceptionCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&DevToolsNetworkInterceptor::GetResponseBody, interceptor_,
                     std::move(interception_id), std::move(callback)));
}

void DevToolsInterceptorController::NavigationStarted(
    const std::string& interception_id,
    const GlobalRequestID& request_id) {
  navigation_requests_[interception_id] = request_id;
}

void DevToolsInterceptorController::NavigationFinished(
    const std::string& interception_id) {
  navigation_requests_.erase(interception_id);
}

// static
DevToolsInterceptorController*
DevToolsInterceptorController::FromBrowserContext(BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return static_cast<DevToolsInterceptorController*>(
      context->GetUserData(kDevToolsInterceptorController));
}

DevToolsInterceptorController::DevToolsInterceptorController(
    base::WeakPtr<DevToolsNetworkInterceptor> interceptor,
    std::unique_ptr<DevToolsTargetRegistry> target_registry,
    BrowserContext* browser_context)
    : interceptor_(interceptor),
      target_registry_(std::move(target_registry)),
      weak_factory_(this) {
  browser_context->SetUserData(
      kDevToolsInterceptorController,
      std::unique_ptr<DevToolsInterceptorController>(this));
}

DevToolsInterceptorController::~DevToolsInterceptorController() = default;

InterceptionHandle::InterceptionHandle(
    DevToolsTargetRegistry::RegistrationHandle registration,
    base::WeakPtr<DevToolsNetworkInterceptor> interceptor,
    DevToolsNetworkInterceptor::FilterEntry* entry)
    : registration_(registration),
      interceptor_(std::move(interceptor)),
      entry_(entry) {}

InterceptionHandle::~InterceptionHandle() {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&DevToolsNetworkInterceptor::RemoveFilterEntry,
                     interceptor_, entry_));
}

void InterceptionHandle::UpdatePatterns(
    std::vector<DevToolsNetworkInterceptor::Pattern> patterns) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&DevToolsNetworkInterceptor::UpdatePatterns, interceptor_,
                     base::Unretained(entry_), std::move(patterns)));
}

}  // namespace content
