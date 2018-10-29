// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/devtools_network_interceptor.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class RenderFrameHostImpl;

class DevToolsURLLoaderInterceptor {
 public:
  class Impl;

  using HandleAuthRequestCallback =
      base::OnceCallback<void(bool use_fallback,
                              const base::Optional<net::AuthCredentials>&)>;
  // Can only be called on the IO thread.
  static void HandleAuthRequest(
      int32_t process_id,
      int32_t routing_id,
      int32_t request_id,
      const scoped_refptr<net::AuthChallengeInfo>& auth_info,
      HandleAuthRequestCallback callback);

  explicit DevToolsURLLoaderInterceptor(
      DevToolsNetworkInterceptor::RequestInterceptedCallback callback);
  ~DevToolsURLLoaderInterceptor();

  void SetPatterns(std::vector<DevToolsNetworkInterceptor::Pattern> patterns);

  void GetResponseBody(
      const std::string& interception_id,
      std::unique_ptr<
          DevToolsNetworkInterceptor::GetResponseBodyForInterceptionCallback>
          callback);
  void TakeResponseBodyPipe(
      const std::string& interception_id,
      DevToolsNetworkInterceptor::TakeResponseBodyPipeCallback callback);
  void ContinueInterceptedRequest(
      const std::string& interception_id,
      std::unique_ptr<DevToolsNetworkInterceptor::Modifications> modifications,
      std::unique_ptr<
          DevToolsNetworkInterceptor::ContinueInterceptedRequestCallback>
          callback);

  bool CreateProxyForInterception(
      RenderFrameHostImpl* rfh,
      bool is_navigation,
      bool is_download,
      network::mojom::URLLoaderFactoryRequest* target_factory_request) const;

 private:
  bool enabled_;
  std::unique_ptr<Impl, base::OnTaskRunnerDeleter> impl_;
  base::WeakPtr<Impl> weak_impl_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsURLLoaderInterceptor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_URL_LOADER_INTERCEPTOR_H_
