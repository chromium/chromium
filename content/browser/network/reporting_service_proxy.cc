// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/reporting_service_proxy.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/network_anonymization_key.h"
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
      const net::NetworkAnonymizationKey& network_anonymization_key)
      : render_process_id_(render_process_id),
        reporting_source_(reporting_source),
        network_anonymization_key_(network_anonymization_key) {
    DCHECK(!reporting_source.is_empty());
  }

  ReportingServiceProxyImpl(const ReportingServiceProxyImpl&) = delete;
  ReportingServiceProxyImpl& operator=(const ReportingServiceProxyImpl&) =
      delete;

  // blink::mojom::ReportingServiceProxy:

  void QueueInterventionReport(const GURL& url,
                               const std::string& id,
                               const std::string& message,
                               const std::optional<std::string>& source_file,
                               int line_number,
                               int column_number) override {
    base::Value::Dict body;
    body.Set("id", id);
    body.Set("message", message);
    if (source_file)
      body.Set("sourceFile", *source_file);
    if (line_number)
      body.Set("lineNumber", line_number);
    if (column_number)
      body.Set("columnNumber", column_number);
    QueueReport(url, "default", "intervention", std::move(body));
  }

  void QueueDeprecationReport(const GURL& url,
                              const std::string& id,
                              std::optional<base::Time> anticipated_removal,
                              const std::string& message,
                              const std::optional<std::string>& source_file,
                              int line_number,
                              int column_number) override {
    base::Value::Dict body;
    body.Set("id", id);
    if (anticipated_removal) {
      body.Set(
          "anticipatedRemoval",
          anticipated_removal->InMillisecondsFSinceUnixEpochIgnoringNull());
    }
    body.Set("message", message);
    if (source_file)
      body.Set("sourceFile", *source_file);
    if (line_number)
      body.Set("lineNumber", line_number);
    if (column_number)
      body.Set("columnNumber", column_number);
    QueueReport(url, "default", "deprecation", std::move(body));
  }

  void QueueCspViolationReport(const GURL& url,
                               const std::string& group,
                               const std::string& document_url,
                               const std::optional<std::string>& referrer,
                               const std::optional<std::string>& blocked_url,
                               const std::string& effective_directive,
                               const std::string& original_policy,
                               const std::optional<std::string>& source_file,
                               const std::optional<std::string>& script_sample,
                               const std::string& disposition,
                               uint16_t status_code,
                               int line_number,
                               int column_number) override {
    base::Value::Dict body;
    body.Set("documentURL", document_url);
    if (referrer)
      body.Set("referrer", *referrer);
    if (blocked_url)
      body.Set("blockedURL", *blocked_url);
    body.Set("effectiveDirective", effective_directive);
    body.Set("originalPolicy", original_policy);
    if (source_file)
      body.Set("sourceFile", *source_file);
    if (script_sample)
      body.Set("sample", *script_sample);
    body.Set("disposition", disposition);
    body.Set("statusCode", status_code);
    if (line_number)
      body.Set("lineNumber", line_number);
    if (column_number)
      body.Set("columnNumber", column_number);
    QueueReport(url, group, "csp-violation", std::move(body));
  }

  void QueueIntegrityViolationReport(const GURL& url,
                                     const std::string& endpoint,
                                     const std::string& document_url,
                                     const std::string& blocked_url,
                                     const std::string& destination,
                                     bool report_only) override {
    base::Value::Dict body;
    body.Set("documentURL", document_url);
    body.Set("blockedURL", blocked_url);
    body.Set("destination", destination);
    body.Set("reportOnly", report_only);
    QueueReport(url, endpoint, "integrity-violation", std::move(body));
  }

  void QueuePermissionsPolicyViolationReport(
      const GURL& url,
      const std::string& endpoint,
      const std::string& policy_id,
      const std::string& disposition,
      const std::optional<std::string>& message,
      const std::optional<std::string>& source_file,
      int line_number,
      int column_number) override {
    base::Value::Dict body;
    body.Set("policyId", policy_id);
    body.Set("disposition", disposition);
    if (message)
      body.Set("message", *message);
    if (source_file)
      body.Set("sourceFile", *source_file);
    if (line_number)
      body.Set("lineNumber", line_number);
    if (column_number)
      body.Set("columnNumber", column_number);
    QueueReport(url, endpoint, "permissions-policy-violation", std::move(body));
  }

  void QueuePotentialPermissionsPolicyViolationReport(
      const GURL& url,
      const std::string& endpoint,
      const std::string& policy_id,
      const std::string& disposition,
      const std::optional<std::string>& message,
      const std::optional<std::string>& allow_attribute,
      const std::optional<std::string>& src_attribute,
      const std::optional<std::string>& source_file,
      int line_number,
      int column_number) override {
    base::Value::Dict body;
    body.Set("policyId", policy_id);
    body.Set("disposition", disposition);
    if (message) {
      body.Set("message", *message);
    }
    if (allow_attribute) {
      body.Set("allowAttribute", *allow_attribute);
    }
    if (src_attribute) {
      body.Set("srcAttribute", *src_attribute);
    }
    if (source_file) {
      body.Set("sourceFile", *source_file);
    }
    if (line_number) {
      body.Set("lineNumber", line_number);
    }
    if (column_number) {
      body.Set("columnNumber", column_number);
    }
    QueueReport(url, endpoint, "potential-permissions-policy-violation",
                std::move(body));
  }

  void QueueDocumentPolicyViolationReport(
      const GURL& url,
      const std::string& group,
      const std::string& policy_id,
      const std::string& disposition,
      const std::optional<std::string>& message,
      const std::optional<std::string>& source_file,
      int line_number,
      int column_number) override {
    base::Value::Dict body;
    body.Set("policyId", policy_id);
    body.Set("disposition", disposition);
    if (message)
      body.Set("message", *message);
    if (source_file)
      body.Set("sourceFile", *source_file);
    if (line_number)
      body.Set("lineNumber", line_number);
    if (column_number)
      body.Set("columnNumber", column_number);
    QueueReport(url, group, "document-policy-violation", std::move(body));
  }

  void QueueCSPHashReport(const GURL& url,
                          const std::string& endpoint,
                          const std::string& subresource_url,
                          const std::string& integrity_hash,
                          const std::string& type,
                          const std::string& destination) override {
    base::Value::Dict body;
    body.Set("documentURL", url.spec());
    body.Set("subresourceURL", subresource_url);
    body.Set("hash", integrity_hash);
    body.Set("type", type);
    body.Set("destination", destination);
    QueueReport(url, endpoint, "csp-hash", std::move(body));
  }

  int render_process_id() const { return render_process_id_; }

 private:
  void QueueReport(const GURL& url,
                   const std::string& group,
                   const std::string& type,
                   base::Value::Dict body) {
    auto* rph = RenderProcessHost::FromID(render_process_id_);
    if (!rph)
      return;
    rph->GetStoragePartition()->GetNetworkContext()->QueueReport(
        type, group, url, reporting_source_, network_anonymization_key_,
        std::move(body));
  }

  const int render_process_id_;
  const base::UnguessableToken reporting_source_;
  const net::NetworkAnonymizationKey network_anonymization_key_;
};

}  // namespace

void CreateReportingServiceProxyForFrame(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ReportingServiceProxy> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ReportingServiceProxyImpl>(
          render_frame_host->GetProcess()->GetDeprecatedID(),
          render_frame_host->GetReportingSource(),
          render_frame_host->GetIsolationInfoForSubresources()
              .network_anonymization_key()),
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
          service_worker_host->GetNetworkAnonymizationKey()),
      std::move(receiver));
}

void CreateReportingServiceProxyForSharedWorker(
    SharedWorkerHost* shared_worker_host,
    mojo::PendingReceiver<blink::mojom::ReportingServiceProxy> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ReportingServiceProxyImpl>(
          shared_worker_host->GetProcessHost()->GetDeprecatedID(),
          shared_worker_host->GetReportingSource(),
          shared_worker_host->GetNetworkAnonymizationKey()),
      std::move(receiver));
}

void CreateReportingServiceProxyForDedicatedWorker(
    DedicatedWorkerHost* dedicated_worker_host,
    mojo::PendingReceiver<blink::mojom::ReportingServiceProxy> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ReportingServiceProxyImpl>(
          dedicated_worker_host->GetProcessHost()->GetDeprecatedID(),
          dedicated_worker_host->GetReportingSource(),
          dedicated_worker_host->GetNetworkAnonymizationKey()),
      std::move(receiver));
}

void CreateReportingServiceProxyForSharedStorageWorklet(
    SharedStorageWorkletHost* shared_storage_worklet_host,
    mojo::PendingReceiver<blink::mojom::ReportingServiceProxy> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(shared_storage_worklet_host->GetProcessHost());
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ReportingServiceProxyImpl>(
          shared_storage_worklet_host->GetProcessHost()->GetDeprecatedID(),
          shared_storage_worklet_host->GetWorkletToken(),
          net::NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
              shared_storage_worklet_host->MaybeGetNetworkIsolationKey())),
      std::move(receiver));
}

}  // namespace content
