// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/blocklist_state_fetcher.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/escape.h"
#include "base/task/single_thread_task_runner.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/crx_info.pb.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/buildflags/buildflags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::BrowserThread;

namespace extensions {

BlocklistStateFetcher::BlocklistStateFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

BlocklistStateFetcher::~BlocklistStateFetcher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void BlocklistStateFetcher::Request(const std::string& id,
                                    RequestCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!safe_browsing_config_) {
    std::optional<safe_browsing::V4ProtocolConfig> config =
        ExtensionsBrowserClient::Get()->GetV4ProtocolConfig();
    if (config) {
      SetSafeBrowsingConfig(*config);
    } else {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), BLOCKLISTED_UNKNOWN));
      return;
    }
  }

  bool request_already_sent = base::Contains(callbacks_, id);
  callbacks_.insert(std::make_pair(id, std::move(callback)));
  if (request_already_sent) {
    return;
  }

  SendRequest(id);
}

void BlocklistStateFetcher::SendRequest(const std::string& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ClientCRXListInfoRequest request;
  request.set_id(id);
  std::string request_str;
  request.SerializeToString(&request_str);

  GURL request_url = GURL(safe_browsing::GetReportUrl(
      *safe_browsing_config_, "clientreport/crx-list-info"));
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("extension_blacklist", R"(
        semantics {
          sender: "Extension Blacklist"
          description:
            "Chromium protects the users from malicious extensions by checking "
            "extensions that are being installed or have been installed "
            "against a list of known malwares. Chromium sends the identifiers "
            "of extensions to Google and Google responds with whether it "
            "believes each extension is malware or not. Only extensions that "
            "match the safe browsing blacklist can trigger this request."
          trigger:
            "When extensions are being installed and at startup when existing "
            "extensions are scanned."
          data: "The identifier of the installed extension."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              owners: "//extensions/OWNERS"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          last_reviewed: "2025-11-12"
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing cookies store"
          setting:
            "Users can enable or disable this feature by toggling 'Protect you "
            "and your device from dangerous sites' in Chromium settings under "
            "Privacy. This feature is enabled by default."
          chrome_policy {
            SafeBrowsingProtectionLevel {
              policy_options {mode: MANDATORY}
              SafeBrowsingProtectionLevel: 0
            }
          }
          chrome_policy {
            SafeBrowsingEnabled {
              policy_options {mode: MANDATORY}
              SafeBrowsingEnabled: false
            }
          }
          deprecated_policies: "SafeBrowsingEnabled"
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = request_url;
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  std::unique_ptr<network::SimpleURLLoader> fetcher_ptr =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  auto* fetcher = fetcher_ptr.get();
  fetcher->AttachStringForUpload(request_str, "application/octet-stream");
  requests_[fetcher] = {std::move(fetcher_ptr), id};
  fetcher->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&BlocklistStateFetcher::OnURLLoaderComplete,
                     base::Unretained(this), fetcher));
}

void BlocklistStateFetcher::SetSafeBrowsingConfig(
    const safe_browsing::V4ProtocolConfig& config) {
  safe_browsing_config_ =
      std::make_unique<safe_browsing::V4ProtocolConfig>(config);
}

void BlocklistStateFetcher::OnURLLoaderComplete(
    network::SimpleURLLoader* url_loader,
    std::unique_ptr<std::string> response_body) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    response_code = url_loader->ResponseInfo()->headers->response_code();
  }

  std::string response_body_str;
  if (response_body.get()) {
    response_body_str = std::move(*response_body.get());
  }

  OnURLLoaderCompleteInternal(url_loader, response_body_str, response_code,
                              url_loader->NetError());
}

void BlocklistStateFetcher::OnURLLoaderCompleteInternal(
    network::SimpleURLLoader* url_loader,
    const std::string& response_body,
    int response_code,
    int net_error) {
  auto it = requests_.find(url_loader);
  if (it == requests_.end()) {
    NOTREACHED();
  }

  std::unique_ptr<network::SimpleURLLoader> loader =
      std::move(it->second.first);
  std::string id = it->second.second;
  requests_.erase(it);

  BlocklistState state;
  if (net_error == net::OK && response_code == 200) {
    ClientCRXListInfoResponse response;
    if (response.ParseFromString(response_body)) {
      state = static_cast<BlocklistState>(response.verdict());
    } else {
      state = BLOCKLISTED_UNKNOWN;
    }
  } else {
    if (net_error != net::OK) {
      VLOG(1) << "Blocklist request for: " << id
              << " failed with error: " << net_error;
    } else {
      VLOG(1) << "Blocklist request for: " << id
              << " failed with error: " << response_code;
    }

    state = BLOCKLISTED_UNKNOWN;
  }

  std::pair<CallbackMultiMap::iterator, CallbackMultiMap::iterator> range =
      callbacks_.equal_range(id);
  for (CallbackMultiMap::iterator callback_it = range.first;
       callback_it != range.second; ++callback_it) {
    std::move(callback_it->second).Run(state);
  }

  callbacks_.erase(range.first, range.second);
}

}  // namespace extensions
