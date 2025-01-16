// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SECURITY_DIP_DOCUMENT_ISOLATION_POLICY_REPORTER_H_
#define CONTENT_BROWSER_SECURITY_DIP_DOCUMENT_ISOLATION_POLICY_REPORTER_H_

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/mojom/document_isolation_policy.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/mojom/frame/reporting_observer.mojom.h"
#include "url/gurl.h"

namespace content {

class StoragePartition;

// Used to report (potential) DocumentIsolationPolicy violations.
// A DocumentIsolationPolicyReporter is retained by an object that represents
// a "setting object" in the browser process such as RenderFrameHostImpl and
// DedicatedWorkerHost. They create a mojo endpoint using Clone and pass it
// around. For example, it's sent to the Network Service via
// network.mojom.URLLoaderFactoryParam.document_isolation_policy_reporter.
// A DocumentIsolationPolicyReporter lives on the UI thread.
class CONTENT_EXPORT DocumentIsolationPolicyReporter final
    : public network::mojom::DocumentIsolationPolicyReporter {
 public:
  DocumentIsolationPolicyReporter(
      base::WeakPtr<StoragePartition> storage_partition,
      const GURL& context_url,
      const std::optional<std::string>& endpoint,
      const std::optional<std::string>& report_only_endpoint,
      const base::UnguessableToken& reporting_source,
      const net::NetworkAnonymizationKey& network_anonymization_key);
  ~DocumentIsolationPolicyReporter() override;
  DocumentIsolationPolicyReporter(const DocumentIsolationPolicyReporter&) =
      delete;
  DocumentIsolationPolicyReporter& operator=(
      const DocumentIsolationPolicyReporter&) = delete;

  // network::mojom::DocumentIsolationPolicyReporter implementation.
  void QueueCorpViolationReport(const GURL& blocked_url,
                                network::mojom::RequestDestination destination,
                                bool report_only) override;
  void Clone(
      mojo::PendingReceiver<network::mojom::DocumentIsolationPolicyReporter>
          receiver) override;

  void BindObserver(
      mojo::PendingRemote<blink::mojom::ReportingObserver> observer);

  base::WeakPtr<DocumentIsolationPolicyReporter> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void QueueAndNotify(
      std::initializer_list<std::pair<std::string_view, std::string_view>> body,
      bool report_only);

  base::WeakPtr<StoragePartition> storage_partition_;

  const GURL context_url_;
  const std::optional<std::string> endpoint_;
  const std::optional<std::string> report_only_endpoint_;
  // This reporting source is not owned by DocumentIsolationPolicyReporter in
  // any way. The DocumentIsolationPolicyReporter is not responsible for
  // cleaning up the reporting source, the actual owner of this token needs to
  // manage the lifecycle (including cleaning up the reporting source from
  // reporting cache).
  base::UnguessableToken reporting_source_;
  const net::NetworkAnonymizationKey network_anonymization_key_;

  mojo::ReceiverSet<network::mojom::DocumentIsolationPolicyReporter>
      receiver_set_;
  mojo::Remote<blink::mojom::ReportingObserver> observer_;

  // This must be the last member.
  base::WeakPtrFactory<DocumentIsolationPolicyReporter> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SECURITY_DIP_DOCUMENT_ISOLATION_POLICY_REPORTER_H_
