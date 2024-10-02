// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network_context_client_base_impl.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/file_access/scoped_file_access.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/network_context_client_base.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"

namespace content {

namespace {

void HandleFileUploadRequest(
    int32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    network::mojom::NetworkContextClient::OnFileUploadRequestedCallback
        callback,
    scoped_refptr<base::TaskRunner> task_runner,
    file_access::ScopedFileAccess scoped_file_access) {
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
    files.emplace_back(file_path, file_flags);
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

void OnScopedFilesAccessAcquired(
    int32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    network::mojom::NetworkContextClient::OnFileUploadRequestedCallback
        callback,
    file_access::ScopedFileAccess scoped_file_access) {
  if (!scoped_file_access.is_allowed()) {
    std::move(callback).Run(net::Error::ERR_ACCESS_DENIED, /*files=*/{});
    return;
  }
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&HandleFileUploadRequest, process_id, async, file_paths,
                     std::move(callback),
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     std::move(scoped_file_access)));
}

void NetworkContextOnFileUploadRequested(
    int32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    const GURL& destination_url,
    network::mojom::NetworkContextClient::OnFileUploadRequestedCallback
        callback) {
  GetContentClient()->browser()->RequestFilesAccess(
      file_paths, destination_url,
      base::BindOnce(&OnScopedFilesAccessAcquired, process_id, async,
                     file_paths, std::move(callback)));
}

NetworkContextClientBase::NetworkContextClientBase() = default;
NetworkContextClientBase::~NetworkContextClientBase() = default;

void NetworkContextClientBase::OnFileUploadRequested(
    int32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    const GURL& destination_url,
    OnFileUploadRequestedCallback callback) {
  NetworkContextOnFileUploadRequested(process_id, async, file_paths,
                                      destination_url, std::move(callback));
}

void NetworkContextClientBase::OnCanSendReportingReports(
    const std::vector<url::Origin>& origins,
    OnCanSendReportingReportsCallback callback) {
  std::move(callback).Run(std::vector<url::Origin>());
}

void NetworkContextClientBase::OnCanSendDomainReliabilityUpload(
    const url::Origin& origin,
    OnCanSendDomainReliabilityUploadCallback callback) {
  std::move(callback).Run(false);
}

#if BUILDFLAG(IS_ANDROID)
void NetworkContextClientBase::OnGenerateHttpNegotiateAuthToken(
    const std::string& server_auth_token,
    bool can_delegate,
    const std::string& auth_negotiate_android_account_type,
    const std::string& spn,
    OnGenerateHttpNegotiateAuthTokenCallback callback) {
  std::move(callback).Run(net::ERR_FAILED, server_auth_token);
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
void NetworkContextClientBase::OnTrustAnchorUsed() {}
#endif

#if BUILDFLAG(IS_CT_SUPPORTED)
void NetworkContextClientBase::OnCanSendSCTAuditingReport(
    OnCanSendSCTAuditingReportCallback callback) {
  std::move(callback).Run(false);
}

void NetworkContextClientBase::OnNewSCTAuditingReportSent() {}
#endif

}  // namespace content
