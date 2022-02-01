// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/reporting_service_proxy.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/mojom/reporting/reporting.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

class ReportingServiceProxyImpl : public blink::mojom::ReportingServiceProxy {
 public:
  ReportingServiceProxyImpl(
      int render_process_id,
      const base::UnguessableToken& reporting_source,
      const net::NetworkIsolationKey& network_isolation_key)
      : render_process_id_(render_process_id),
        reporting_source_(reporting_source),
        network_isolation_key_(network_isolation_key) {
    DCHECK(!reporting_source.is_empty());
  }

  ReportingServiceProxyImpl(const ReportingServiceProxyImpl&) = delete;
  ReportingServiceProxyImpl& operator=(const ReportingServiceProxyImpl&) =
      delete;

  // blink::mojom::ReportingServiceProxy:

  void QueueInterventionReport(const GURL& url,
                               const std::string& id,
                               const std::string& message,
                               const absl::optional<std::string>& source_file,
                               int line_number,
                               int column_number) override {
    auto body = std::make_unique<base::DictionaryValue>();
    body->SetString("id", id);
    body->SetString("message", message);
    if (source_file)
      body->SetString("sourceFile", *source_file);
    if (line_number)
      body->SetInteger("lineNumber", line_number);
    if (column_number)
      body->SetInteger("columnNumber", column_number);
    QueueReport(url, "default", "intervention", std::move(body));
  }

  void QueueDeprecationReport(const GURL& url,
                              const std::string& id,
                              absl::optional<base::Time> anticipated_removal,
                              const std::string& message,
                              const absl::optional<std::string>& source_file,
                              int line_number,
                              int column_number) override {
    auto body = std::make_unique<base::DictionaryValue>();
    body->SetString("id", id);
    if (anticipated_removal)
      body->SetDouble("anticipatedRemoval",
                      anticipated_removal->ToJsTimeIgnoringNull());
    body->SetString("message", message);
    if (source_file)
      body->SetString("sourceFile", *source_file);
    if (line_number)
      body->SetInteger("lineNumber", line_number);
    if (column_number)
      body->SetInteger("columnNumber", column_number);
    QueueReport(url, "default", "deprecation", std::move(body));
  }

  void QueueCspViolationReport(const GURL& url,
                               const std::string& group,
                               const std::string& document_url,
                               const absl::optional<std::string>& referrer,
                               const absl::optional<std::string>& blocked_url,
                               const std::string& effective_directive,
                               const std::string& original_policy,
                               const absl::optional<std::string>& source_file,
                               const absl::optional<std::string>& script_sample,
                               const std::string& disposition,
                               uint16_t status_code,
                               int line_number,
                               int column_number) override {
    auto body = std::make_unique<base::DictionaryValue>();
    body->SetString("documentURL", document_url);
    if (referrer)
      body->SetString("referrer", *referrer);
    if (blocked_url)
      body->SetString("blockedURL", *blocked_url);
    body->SetString("effectiveDirective", effective_directive);
    body->SetString("originalPolicy", original_policy);
    if (source_file)
      body->SetString("sourceFile", *source_file);
    if (script_sample)
      body->SetString("sample", *script_sample);
    body->SetString("disposition", disposition);
    body->SetInteger("statusCode", status_code);
    if (line_number)
      body->SetInteger("lineNumber", line_number);
    if (column_number)
      body->SetInteger("columnNumber", column_number);
    QueueReport(url, group, "csp-violation", std::move(body));
  }

  void QueuePermissionsPolicyViolationReport(
      const GURL& url,
      const std::string& policy_id,
      const std::string& disposition,
      const absl::optional<std::string>& message,
      const absl::optional<std::string>& source_file,
      int line_number,
      int column_number) override {
    auto body = std::make_unique<base::DictionaryValue>();
    body->SetString("policyId", policy_id);
    body->SetString("disposition", disposition);
    if (message)
      body->SetString("message", *message);
    if (source_file)
      body->SetString("sourceFile", *source_file);
    if (line_number)
      body->SetInteger("lineNumber", line_number);
    if (column_number)
      body->SetInteger("columnNumber", column_number);
    QueueReport(url, "default", "permissions-policy-violation",
                std::move(body));
  }

  void QueueDocumentPolicyViolationReport(
      const GURL& url,
      const std::string& group,
      const std::string& policy_id,
      const std::string& disposition,
      const absl::optional<std::string>& message,
      const absl::optional<std::string>& source_file,
      int line_number,
      int column_number) override {
    auto body = std::make_unique<base::DictionaryValue>();
    body->SetString("policyId", policy_id);
    body->SetString("disposition", disposition);
    if (message)
      body->SetString("message", *message);
    if (source_file)
      body->SetString("sourceFile", *source_file);
    if (line_number)
      body->SetInteger("lineNumber", line_number);
    if (column_number)
      body->SetInteger("columnNumber", column_number);
    QueueReport(url, group, "document-policy-violation", std::move(body));
  }

  int render_process_id() const { return render_process_id_; }

 private:
  void QueueReport(const GURL& url,
                   const std::string& group,
                   const std::string& type,
                   std::unique_ptr<base::Value> body) {
    auto* rph = RenderProcessHost::FromID(render_process_id_);
    if (!rph)
      return;
    rph->GetStoragePartition()->GetNetworkContext()->QueueReport(
        type, group, url, reporting_source_, network_isolation_key_,
        /*user_agent=*/absl::nullopt,
        base::Value::FromUniquePtrValue(std::move(body)));
  }

  const int render_process_id_;
  const base::UnguessableToken reporting_source_;
  const net::NetworkIsolationKey network_isolation_key_;
};

}  // namespace

void CreateReportingServiceProxyForFrame(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ReportingServiceProxy> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojo::MakeSelfOwnedReceiver(std::make_unique<ReportingServiceProxyImpl>(
                                  render_frame_host->GetProcess()->GetID(),
                                  render_frame_host->GetReportingSource(),
                                  render_frame_host->GetNetworkIsolationKey()),
                              std::move(receiver));
}

void CreateReportingServiceProxyForServiceWorker(
    ServiceWorkerHost* service_worker_host,
    mojo::PendingReceiver<blink::mojom::ReportingServiceProxy> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ReportingServiceProxyImpl>(
          service_worker_host->worker_process_id(),
          service_worker_host->GetReportingSource(),
          service_worker_host->GetNetworkIsolationKey()),
      std::move(receiver));
}

void CreateReportingServiceProxyForSharedWorker(
    SharedWorkerHost* shared_worker_host,
    mojo::PendingReceiver<blink::mojom::ReportingServiceProxy> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojo::MakeSelfOwnedReceiver(std::make_unique<ReportingServiceProxyImpl>(
                                  shared_worker_host->GetProcessHost()->GetID(),
                                  shared_worker_host->GetReportingSource(),
                                  shared_worker_host->GetNetworkIsolationKey()),
                              std::move(receiver));
}

void CreateReportingServiceProxyForDedicatedWorker(
    DedicatedWorkerHost* dedicated_worker_host,
    mojo::PendingReceiver<blink::mojom::ReportingServiceProxy> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ReportingServiceProxyImpl>(
          dedicated_worker_host->GetProcessHost()->GetID(),
          dedicated_worker_host->GetReportingSource(),
          dedicated_worker_host->GetNetworkIsolationKey()),
      std::move(receiver));
}

}  // namespace content
