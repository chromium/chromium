// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/cross_origin_embedder_policy_reporter.h"

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

namespace {

constexpr char kType[] = "coep";

GURL StripUsernameAndPassword(const GURL& url) {
  url::Replacements<char> replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  return url.ReplaceComponents(replacements);
}

}  // namespace

CrossOriginEmbedderPolicyReporter::CrossOriginEmbedderPolicyReporter(
    StoragePartition* storage_partition,
    const GURL& context_url,
    const absl::optional<std::string>& endpoint,
    const absl::optional<std::string>& report_only_endpoint,
    const net::NetworkIsolationKey& network_isolation_key)
    : storage_partition_(storage_partition),
      context_url_(context_url),
      endpoint_(endpoint),
      report_only_endpoint_(report_only_endpoint),
      network_isolation_key_(network_isolation_key) {
  DCHECK(storage_partition_);
}

CrossOriginEmbedderPolicyReporter::~CrossOriginEmbedderPolicyReporter() =
    default;

void CrossOriginEmbedderPolicyReporter::QueueCorpViolationReport(
    const GURL& blocked_url,
    network::mojom::RequestDestination destination,
    bool report_only) {
  GURL url_to_pass = StripUsernameAndPassword(blocked_url);
  QueueAndNotify(
      {std::make_pair("type", "corp"),
       std::make_pair("blockedURL", url_to_pass.spec()),
       std::make_pair("destination",
                      network::RequestDestinationToString(destination))},
      report_only);
}

void CrossOriginEmbedderPolicyReporter::BindObserver(
    mojo::PendingRemote<blink::mojom::ReportingObserver> observer) {
  observer_.Bind(std::move(observer));
}

void CrossOriginEmbedderPolicyReporter::QueueNavigationReport(
    const GURL& blocked_url,
    bool report_only) {
  GURL url_to_pass = StripUsernameAndPassword(blocked_url);
  QueueAndNotify({std::make_pair("type", "navigation"),
                  std::make_pair("blockedURL", url_to_pass.spec())},
                 report_only);
}

void CrossOriginEmbedderPolicyReporter::QueueWorkerInitializationReport(
    const GURL& blocked_url,
    bool report_only) {
  GURL url_to_pass = StripUsernameAndPassword(blocked_url);
  QueueAndNotify({std::make_pair("type", "worker initialization"),
                  std::make_pair("blockedURL", url_to_pass.spec())},
                 report_only);
}

void CrossOriginEmbedderPolicyReporter::Clone(
    mojo::PendingReceiver<network::mojom::CrossOriginEmbedderPolicyReporter>
        receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

void CrossOriginEmbedderPolicyReporter::QueueAndNotify(
    std::initializer_list<std::pair<base::StringPiece, base::StringPiece>> body,
    bool report_only) {
  const absl::optional<std::string>& endpoint =
      report_only ? report_only_endpoint_ : endpoint_;
  const char* const disposition = report_only ? "reporting" : "enforce";
  if (observer_) {
    std::vector<blink::mojom::ReportBodyElementPtr> list;

    for (const auto& pair : body) {
      list.push_back(blink::mojom::ReportBodyElement::New(
          std::string(pair.first), std::string(pair.second)));
    }
    list.push_back(
        blink::mojom::ReportBodyElement::New("disposition", disposition));

    observer_->Notify(blink::mojom::Report::New(
        kType, context_url_, blink::mojom::ReportBody::New(std::move(list))));
  }
  if (endpoint) {
    base::DictionaryValue body_to_pass;
    for (const auto& pair : body) {
      body_to_pass.SetString(pair.first, pair.second);
    }
    body_to_pass.SetString("disposition", disposition);

    storage_partition_->GetNetworkContext()->QueueReport(
        kType, *endpoint, context_url_, network_isolation_key_,
        /*user_agent=*/absl::nullopt, std::move(body_to_pass));
  }
}

}  // namespace content
