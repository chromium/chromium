// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network_context_client_base_impl.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/network_context_client_base.h"
#include "mojo/public/cpp/bindings/remote.h"

#if defined(OS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif

namespace content {

namespace {

void HandleFileUploadRequest(
    int32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    network::mojom::NetworkContextClient::OnFileUploadRequestedCallback
        callback,
    scoped_refptr<base::TaskRunner> task_runner) {
  std::vector<base::File> files;
  uint32_t file_flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
                        (async ? base::File::FLAG_ASYNC : 0);
  ChildProcessSecurityPolicy* cpsp = ChildProcessSecurityPolicy::GetInstance();
  for (const auto& file_path : file_paths) {
    if (process_id != network::mojom::kBrowserProcessId &&
        !cpsp->CanReadFile(process_id, file_path)) {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), net::ERR_ACCESS_DENIED,
                                    std::vector<base::File>()));
      return;
    }
#if defined(OS_ANDROID)
    if (file_path.IsContentUri()) {
      files.push_back(base::OpenContentUriForRead(file_path));
    } else {
      files.emplace_back(file_path, file_flags);
    }
#else
    files.emplace_back(file_path, file_flags);
#endif
    if (!files.back().IsValid()) {
      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback),
                         net::FileErrorToNetError(files.back().error_details()),
                         std::vector<base::File>()));
      return;
    }
  }
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), net::OK,
                                                  std::move(files)));
}

}  // namespace

void NetworkContextOnFileUploadRequested(
    int32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    network::mojom::NetworkContextClient::OnFileUploadRequestedCallback
        callback) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&HandleFileUploadRequest, process_id, async, file_paths,
                     std::move(callback),
                     base::SequencedTaskRunnerHandle::Get()));
}

NetworkContextClientBase::NetworkContextClientBase() = default;
NetworkContextClientBase::~NetworkContextClientBase() = default;

void NetworkContextClientBase::OnFileUploadRequested(
    int32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    OnFileUploadRequestedCallback callback) {
  NetworkContextOnFileUploadRequested(process_id, async, file_paths,
                                      std::move(callback));
}

void NetworkContextClientBase::OnCanSendReportingReports(
    const std::vector<url::Origin>& origins,
    OnCanSendReportingReportsCallback callback) {
  std::move(callback).Run(std::vector<url::Origin>());
}

void NetworkContextClientBase::OnCanSendDomainReliabilityUpload(
    const GURL& origin,
    OnCanSendDomainReliabilityUploadCallback callback) {
  std::move(callback).Run(false);
}

#if defined(OS_ANDROID)
void NetworkContextClientBase::OnGenerateHttpNegotiateAuthToken(
    const std::string& server_auth_token,
    bool can_delegate,
    const std::string& auth_negotiate_android_account_type,
    const std::string& spn,
    OnGenerateHttpNegotiateAuthTokenCallback callback) {
  std::move(callback).Run(net::ERR_FAILED, server_auth_token);
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
void NetworkContextClientBase::OnTrustAnchorUsed() {}
#endif

void NetworkContextClientBase::OnTrustTokenIssuanceDivertedToSystem(
    network::mojom::FulfillTrustTokenIssuanceRequestPtr request,
    OnTrustTokenIssuanceDivertedToSystemCallback callback) {
  auto response = network::mojom::FulfillTrustTokenIssuanceAnswer::New();
  response->status =
      network::mojom::FulfillTrustTokenIssuanceAnswer::Status::kNotFound;
  std::move(callback).Run(std::move(response));
}

}  // namespace content
