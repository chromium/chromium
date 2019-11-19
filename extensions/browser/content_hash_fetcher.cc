// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_hash_fetcher.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/content_verifier/content_hash.h"
#include "extensions/browser/content_verifier_delegate.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/verified_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace extensions {

namespace internals {

ContentHashFetcher::ContentHashFetcher(ContentHash::FetchKey key)
    : fetch_key_(std::move(key)),
      response_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

void ContentHashFetcher::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "URLFetchComplete for " << fetch_key_.extension_id
          << " is_success:" << !!response_body << " "
          << fetch_key_.fetch_url.possibly_invalid_spec();
  DCHECK(hash_fetcher_callback_);
  response_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(hash_fetcher_callback_), std::move(fetch_key_),
                     std::move(response_body)));
  delete this;
}

void ContentHashFetcher::Start(HashFetcherCallback hash_fetcher_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  hash_fetcher_callback_ = std::move(hash_fetcher_callback);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("content_hash_verification_job", R"(
        semantics {
          sender: "Web Store Content Verification"
          description:
            "The request sent to retrieve the file required for content "
            "verification for an extension from the Web Store."
          trigger:
            "An extension from the Web Store is missing the "
            "verified_contents.json file required for extension content "
            "verification."
          data: "The extension id and extension version."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be directly disabled; it is enabled if any "
            "extension from the webstore is installed in the browser."
          policy_exception_justification:
            "Not implemented, not required. If the user has extensions from "
            "the Web Store, this feature is required to ensure the "
            "extensions match what is distributed by the store."
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = fetch_key_.fetch_url;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_remote(
      std::move(fetch_key_.url_loader_factory_remote));

  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);
  const int kMaxRetries = 3;
  simple_loader_->SetRetryOptions(
      kMaxRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_remote.get(),
      base::BindOnce(&ContentHashFetcher::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

ContentHashFetcher::~ContentHashFetcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace internals
}  // namespace extensions
