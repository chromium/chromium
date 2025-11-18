// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_NETWORK_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_WEBID_NETWORK_REQUEST_MANAGER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace content::webid {

using DownloadCallback =
    base::OnceCallback<void(std::optional<std::string> response_body,
                            int response_code,
                            const std::string& mime_type,
                            bool cors_error)>;
enum class ParseStatus {
  kSuccess,
  kHttpNotFoundError,
  kNoResponseError,
  kInvalidResponseError,
  // ParseStatus::kEmptyListError only applies to well known and account list
  // responses. It is used to classify a successful response where the list in
  // the response is empty.
  kEmptyListError,
  kInvalidContentTypeError,
};

struct FetchStatus {
  ParseStatus parse_status;
  int response_code;
  bool cors_error = false;
  // Whether this fetch was fulfilled by the accounts pushed to the browser.
  // Only set to true for accounts fetches in the FedCM API where lightweight
  // FedCM is enabled.
  bool from_accounts_push = false;
};

using ParseJsonCallback =
    base::OnceCallback<void(FetchStatus,
                            data_decoder::DataDecoder::ValueOrError)>;

GURL ExtractEndpoint(const GURL& provider,
                     const base::Value::Dict& response,
                     const char* key);

CONTENT_EXPORT std::optional<GURL> ComputeWellKnownUrl(const GURL& provider,
                                                       const std::string& path);

// Base class containing some methods for creating fetches in webid APIs.
class CONTENT_EXPORT NetworkRequestManager {
 public:
  NetworkRequestManager(
      const url::Origin& relying_party_origin,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      network::mojom::ClientSecurityStatePtr client_security_state,
      network::mojom::RequestDestination destination,
      content::FrameTreeNodeId frame_tree_node_id);
  virtual ~NetworkRequestManager();

  NetworkRequestManager(const NetworkRequestManager&) = delete;
  NetworkRequestManager& operator=(const NetworkRequestManager&) = delete;

  enum class CredentialedResourceRequestType {
    kNoOrigin,
    kOriginWithoutCORS,
    kOriginWithCORS
  };

 protected:
  virtual net::NetworkTrafficAnnotationTag CreateTrafficAnnotation() = 0;

  // Starts download request using `url_loader`. Calls `parse_json_callback`
  // when the download result has been parsed.
  void DownloadJsonAndParse(
      std::unique_ptr<network::ResourceRequest> resource_request,
      std::optional<std::string> url_encoded_post_data,
      ParseJsonCallback parse_json_callback,
      bool allow_http_error_results = false);

  // Starts download result using `url_loader`. Calls `download_callback` when
  // the download completes.
  void DownloadUrl(std::unique_ptr<network::ResourceRequest> resource_request,
                   std::optional<std::string> url_encoded_post_data,
                   DownloadCallback download_callback,
                   size_t max_download_size,
                   bool allow_http_error_results = false);

  // Called when download initiated by DownloadUrl() completes.
  void OnDownloadedUrl(std::unique_ptr<network::SimpleURLLoader> url_loader,
                       DownloadCallback callback,
                       std::optional<std::string> response_body);

  std::unique_ptr<network::ResourceRequest> CreateUncredentialedResourceRequest(
      const GURL& target_url,
      bool send_origin,
      bool follow_redirects = false) const;

  std::unique_ptr<network::ResourceRequest> CreateCredentialedResourceRequest(
      const GURL& target_url,
      CredentialedResourceRequestType type) const;

  url::Origin relying_party_origin_;

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  network::mojom::ClientSecurityStatePtr client_security_state_;
  const network::mojom::RequestDestination destination_;
  const content::FrameTreeNodeId frame_tree_node_id_;

 private:
  // Maps each SimpleURLLoader instance to a unique, unguessable token
  // (request_id) used for tracking and associating network requests
  // with DevTools instrumentation.
  base::flat_map<network::SimpleURLLoader*, base::UnguessableToken>
      urlloader_devtools_request_id_map_;

  base::WeakPtrFactory<NetworkRequestManager> weak_ptr_factory_{this};
};

}  // namespace content::webid

#endif  // CONTENT_BROWSER_WEBID_NETWORK_REQUEST_MANAGER_H_
