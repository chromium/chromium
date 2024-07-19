// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/cross_origin_embedder_policy_reporter.h"

#include <string_view>

#include "base/values.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

namespace {

constexpr char kType[] = "coep";

GURL StripUsernameAndPassword(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  return url.ReplaceComponents(replacements);
}

}  // namespace

CrossOriginEmbedderPolicyReporter::CrossOriginEmbedderPolicyReporter(
    base::WeakPtr<StoragePartition> storage_partition,
    const GURL& context_url,
    const std::optional<std::string>& endpoint,
    const std::optional<std::string>& report_only_endpoint,
    const base::UnguessableToken& reporting_source,
    const net::NetworkAnonymizationKey& network_anonymization_key)
    : storage_partition_(std::move(storage_partition)),
      context_url_(context_url),
      endpoint_(endpoint),
      report_only_endpoint_(report_only_endpoint),
      reporting_source_(reporting_source),
      network_anonymization_key_(network_anonymization_key) {
  DCHECK(storage_partition_);
  DCHECK(!reporting_source_.is_empty());
}

CrossOriginEmbedderPolicyReporter::~CrossOriginEmbedderPolicyReporter() =
    default;

void CrossOriginEmbedderPolicyReporter::set_reporting_source(
    const base::UnguessableToken& reporting_source) {
  reporting_source_ = reporting_source;
}

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
    std::initializer_list<std::pair<std::string_view, std::string_view>> body,
    bool report_only) {
  const std::optional<std::string>& endpoint =
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
    base::Value::Dict body_to_pass;
    for (const auto& pair : body) {
      body_to_pass.Set(pair.first, pair.second);
    }
    body_to_pass.Set("disposition", disposition);

    if (auto* storage_partition = storage_partition_.get()) {
      storage_partition->GetNetworkContext()->QueueReport(
          kType, *endpoint, context_url_, reporting_source_,
          network_anonymization_key_, std::move(body_to_pass));
    }
  }
}

}  // namespace content
