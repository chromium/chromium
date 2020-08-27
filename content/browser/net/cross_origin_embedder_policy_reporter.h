// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NET_CROSS_ORIGIN_EMBEDDER_POLICY_REPORTER_H_
#define CONTENT_BROWSER_NET_CROSS_ORIGIN_EMBEDDER_POLICY_REPORTER_H_

#include <initializer_list>
#include <string>

#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/mojom/frame/reporting_observer.mojom.h"
#include "url/gurl.h"

namespace content {

class StoragePartition;

// Used to report (potential) COEP violations.
// A CrossOriginEmbedderPolicyReporter is retained by an object that represents
// a "setting object" in the browser process such as RenderFrameHostImpl and
// DedicatedWorkerHost. They create a mojo endpoint using Clone and pass it
// around. For example, it's sent to the Network Service via
// network.mojom.URLLoaderFactoryParam.coep_reporter.
// Any functions other than the destructor must not be called after the
// associated StoragePartition is destructed.
// TODO(yhirano): This currently only sends reports to the network. Notify
// the event to the associated ReportingObserver.
class CONTENT_EXPORT CrossOriginEmbedderPolicyReporter final
    : public network::mojom::CrossOriginEmbedderPolicyReporter {
 public:
  CrossOriginEmbedderPolicyReporter(
      StoragePartition* storage_partition,
      const GURL& context_url,
      const base::Optional<std::string>& endpoint,
      const base::Optional<std::string>& report_only_endpoint);
  ~CrossOriginEmbedderPolicyReporter() override;
  CrossOriginEmbedderPolicyReporter(const CrossOriginEmbedderPolicyReporter&) =
      delete;
  CrossOriginEmbedderPolicyReporter& operator=(
      const CrossOriginEmbedderPolicyReporter&) = delete;

  // network::mojom::CrossOriginEmbedderPolicyReporter implementation.
  void QueueCorpViolationReport(const GURL& blocked_url,
                                network::mojom::RequestDestination destination,
                                bool report_only) override;
  void Clone(
      mojo::PendingReceiver<network::mojom::CrossOriginEmbedderPolicyReporter>
          receiver) override;

  void BindObserver(
      mojo::PendingRemote<blink::mojom::ReportingObserver> observer);

  // https://mikewest.github.io/corpp/#abstract-opdef-queue-coep-navigation-violation
  // Queue a violation report for COEP mismatch for nested frame navigation.
  void QueueNavigationReport(const GURL& blocked_url, bool report_only);

 private:
  void QueueAndNotify(std::initializer_list<
                          std::pair<base::StringPiece, base::StringPiece>> body,
                      bool report_only);

  // See the class comment.
  StoragePartition* const storage_partition_;

  const GURL context_url_;
  const base::Optional<std::string> endpoint_;
  const base::Optional<std::string> report_only_endpoint_;

  mojo::ReceiverSet<network::mojom::CrossOriginEmbedderPolicyReporter>
      receiver_set_;
  mojo::Remote<blink::mojom::ReportingObserver> observer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NET_CROSS_ORIGIN_EMBEDDER_POLICY_REPORTER_H_
