// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_loader_util.h"

#include "base/containers/enum_set.h"
#include "base/metrics/histogram_functions.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request.h"
#include "services/network/ad_heuristic_cookie_overrides.h"
#include "services/network/chunked_data_pipe_upload_data_stream.h"
#include "services/network/data_pipe_element_reader.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_request.mojom-shared.h"

namespace network::url_loader_util {

namespace {

// TODO(https://crbug.com/375352611): add the check for enabling third-party
// cookies.
constexpr uint64_t kAllowedDevToolsCookieSettingOverrides =
    1u << static_cast<int>(
        net::CookieSettingOverride::kForceDisableThirdPartyCookies) |
    1u << static_cast<int>(
        net::CookieSettingOverride::kForceEnableThirdPartyCookieMitigations) |
    1u << static_cast<int>(net::CookieSettingOverride::kSkipTPCDMetadataGrant) |
    1u << static_cast<int>(
        net::CookieSettingOverride::kSkipTPCDHeuristicsGrant);

bool IsMultiplexedConnection(const net::HttpResponseInfo& response_info) {
  switch (net::HttpConnectionInfoToCoarse(response_info.connection_info)) {
    case net::HttpConnectionInfoCoarse::kHTTP1:
      return false;
    case net::HttpConnectionInfoCoarse::kHTTP2:
    case net::HttpConnectionInfoCoarse::kQUIC:
      return true;
    case net::HttpConnectionInfoCoarse::kOTHER:
      return false;
  }
}

const char* GetDestinationTypePartString(
    network::mojom::RequestDestination destination) {
  if (destination == network::mojom::RequestDestination::kDocument) {
    return "MainFrame";
  } else if (destination == network::mojom::RequestDestination::kFrame ||
             destination == network::mojom::RequestDestination::kIframe) {
    return "SubFrame";
  }
  return "Subresource";
}

const char* GetCertStatePartString(const net::SSLInfo& ssl_info) {
  if (!ssl_info.cert.get()) {
    return "NoCert";
  }
  return ssl_info.is_issued_by_known_root ? "KnownRootCert" : "UnknownRootCert";
}

// A subclass of net::UploadBytesElementReader which owns
// ResourceRequestBody.
class BytesElementReader : public net::UploadBytesElementReader {
 public:
  BytesElementReader(ResourceRequestBody* resource_request_body,
                     const DataElementBytes& element)
      : net::UploadBytesElementReader(element.bytes()),
        resource_request_body_(resource_request_body) {}

  BytesElementReader(const BytesElementReader&) = delete;
  BytesElementReader& operator=(const BytesElementReader&) = delete;

  ~BytesElementReader() override = default;

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;
};

// A subclass of net::UploadFileElementReader which owns
// ResourceRequestBody.
// This class is necessary to ensure the BlobData and any attached shareable
// files survive until upload completion.
class FileElementReader : public net::UploadFileElementReader {
 public:
  FileElementReader(ResourceRequestBody* resource_request_body,
                    base::TaskRunner* task_runner,
                    const DataElementFile& element,
                    base::File&& file)
      : net::UploadFileElementReader(task_runner,
                                     std::move(file),
                                     element.path(),
                                     element.offset(),
                                     element.length(),
                                     element.expected_modification_time()),
        resource_request_body_(resource_request_body) {}

  FileElementReader(const FileElementReader&) = delete;
  FileElementReader& operator=(const FileElementReader&) = delete;

  ~FileElementReader() override = default;

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;
};

}  // namespace
std::unique_ptr<net::UploadDataStream> CreateUploadDataStream(
    ResourceRequestBody* body,
    std::vector<base::File>& opened_files,
    base::SequencedTaskRunner* file_task_runner) {
  // In the case of a chunked upload, there will just be one element.
  if (body->elements()->size() == 1) {
    if (body->elements()->begin()->type() ==
        network::mojom::DataElementDataView::Tag::kChunkedDataPipe) {
      auto& element =
          body->elements_mutable()->at(0).As<DataElementChunkedDataPipe>();
      const bool has_null_source = element.read_only_once().value();
      auto upload_data_stream =
          std::make_unique<ChunkedDataPipeUploadDataStream>(
              body, element.ReleaseChunkedDataPipeGetter(), has_null_source);
      if (element.read_only_once()) {
        upload_data_stream->EnableCache();
      }
      return upload_data_stream;
    }
  }

  auto opened_file = opened_files.begin();
  std::vector<std::unique_ptr<net::UploadElementReader>> element_readers;
  for (const auto& element : *body->elements()) {
    switch (element.type()) {
      case network::mojom::DataElementDataView::Tag::kBytes:
        element_readers.push_back(std::make_unique<BytesElementReader>(
            body, element.As<DataElementBytes>()));
        break;
      case network::mojom::DataElementDataView::Tag::kFile:
        CHECK(opened_file != opened_files.end(), base::NotFatalUntil::M130);
        element_readers.push_back(std::make_unique<FileElementReader>(
            body, file_task_runner, element.As<network::DataElementFile>(),
            std::move(*opened_file++)));
        break;
      case network::mojom::DataElementDataView::Tag::kDataPipe: {
        element_readers.push_back(std::make_unique<DataPipeElementReader>(
            body,
            element.As<network::DataElementDataPipe>().CloneDataPipeGetter()));
        break;
      }
      case network::mojom::DataElementDataView::Tag::kChunkedDataPipe: {
        // This shouldn't happen, as the traits logic should ensure that if
        // there's a chunked pipe, there's one and only one element.
        NOTREACHED();
      }
    }
  }
  DCHECK(opened_file == opened_files.end());

  return std::make_unique<net::ElementsUploadDataStream>(
      std::move(element_readers), body->identifier());
}

net::CookieSettingOverrides CalculateCookieSettingOverrides(
    net::CookieSettingOverrides factory_overrides,
    net::CookieSettingOverrides devtools_overrides,
    const ResourceRequest& request,
    bool emit_metrics) {
  net::CookieSettingOverrides overrides(factory_overrides);
  if (request.is_outermost_main_frame &&
      network::cors::IsCorsEnabledRequestMode(request.mode)) {
    overrides.Put(
        net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible);
  }

  AddAdsHeuristicCookieSettingOverrides(request.is_ad_tagged, overrides,
                                        emit_metrics);
  // Only apply the DevTools overrides if the request is from devtools enabled
  // context.
  if (request.devtools_request_id.has_value()) {
    CHECK_EQ(devtools_overrides.ToEnumBitmask() &
                 ~kAllowedDevToolsCookieSettingOverrides,
             0u);
    overrides = base::Union(overrides, devtools_overrides);
  }

  // The `kStorageAccessGrantEligible` override should not be present in
  // factory_overrides.
  CHECK(
      !overrides.Has(net::CookieSettingOverride::kStorageAccessGrantEligible));
  // Add the Storage Access override enum based on whether the request's url and
  // initiator are same-site, to prevent cross-site sibling iframes benefit from
  // each other's storage access API grants. This must be updated on redirects.
  if (net::cookie_util::ShouldAddInitialStorageAccessApiOverride(
          request.url, request.storage_access_api_status,
          request.request_initiator, emit_metrics,
          request.credentials_mode == mojom::CredentialsMode::kInclude)) {
    overrides.Put(net::CookieSettingOverride::kStorageAccessGrantEligible);
  }

  // The `kStorageAccessGrantEligibleViaHeader` override will be applied
  // (in-place) by individual request jobs as appropriate, but should not be
  // present initially.
  CHECK(!overrides.Has(
      net::CookieSettingOverride::kStorageAccessGrantEligibleViaHeader));

  return overrides;
}

std::optional<net::IsolationInfo> GetIsolationInfo(
    const net::IsolationInfo& factory_isolation_info,
    bool automatically_assign_isolation_info,
    const ResourceRequest& request) {
  if (!factory_isolation_info.IsEmpty()) {
    return factory_isolation_info;
  }

  if (request.trusted_params &&
      !request.trusted_params->isolation_info.IsEmpty()) {
    if (request.credentials_mode != network::mojom::CredentialsMode::kOmit) {
      DCHECK(request.trusted_params->isolation_info.site_for_cookies()
                 .IsEquivalent(request.site_for_cookies));
    }
    return request.trusted_params->isolation_info;
  }

  if (automatically_assign_isolation_info) {
    url::Origin origin = url::Origin::Create(request.url);
    return net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                      origin, origin, net::SiteForCookies());
  }

  return std::nullopt;
}

// Retrieves the Cookie header from either `cors_exempt_headers` or `headers`.
std::string GetCookiesFromHeaders(
    const net::HttpRequestHeaders& headers,
    const net::HttpRequestHeaders& cors_exempt_headers) {
  std::optional<std::string> cookies =
      cors_exempt_headers.GetHeader(net::HttpRequestHeaders::kCookie);
  if (!cookies) {
    cookies = headers.GetHeader(net::HttpRequestHeaders::kCookie);
  }
  return std::move(cookies).value_or(std::string());
}

void RecordURLLoaderRequestMetrics(const net::URLRequest& url_request,
                                   size_t raw_request_line_size,
                                   size_t raw_request_headers_size) {
  // All histograms recorded here are of the form:
  // "NetworkService.Requests.{Multiplexed}.{RequestType}.{Method}.{Result}
  // .{Metric}".
  // For example:
  // "NetworkService.Requests.Simple.MainFrame.Get.Success.TotalRequestSize".
  absl::InlinedVector<std::string_view, 10> histogram_prefix_pieces = {
      "NetworkService", "Requests"};

  const net::HttpResponseInfo& response_info = url_request.response_info();
  if (IsMultiplexedConnection(response_info)) {
    histogram_prefix_pieces.push_back("Multiplexed");
  } else {
    histogram_prefix_pieces.push_back("Simple");
  }

  switch (url_request.isolation_info().request_type()) {
    case net::IsolationInfo::RequestType::kMainFrame:
      histogram_prefix_pieces.push_back("MainFrame");
      break;
    case net::IsolationInfo::RequestType::kSubFrame:
    case net::IsolationInfo::RequestType::kOther:
      // TODO(crbug.com/362787712): Add metrics for other types of requests.
      return;
  }

  if (url_request.method() == "GET") {
    histogram_prefix_pieces.push_back("Get");
  } else {
    // Other types of requests need to be handled differently e.g. the total
    // request size of a POST request needs to include the body.
    // TODO(crbug.com/362787712): Add metrics for other types of requests.
    return;
  }

  const int response_code = response_info.headers->response_code();
  if (response_code < 199) {
    // Ignore information responses because they are not complete requests.
    return;
  } else if (response_code < 299 || response_code < 399) {
    // We consider redirects a success.
    histogram_prefix_pieces.push_back("Success");
  } else if (response_code < 499) {
    histogram_prefix_pieces.push_back("ClientError");
  } else if (response_code < 599) {
    histogram_prefix_pieces.push_back("ServerError");
  } else {
    // Ignore unexpected server response codes.
    return;
  }

  auto make_histogram_name =
      [&histogram_prefix_pieces](std::string_view metric) {
        histogram_prefix_pieces.push_back(metric);
        std::string name = base::JoinString(histogram_prefix_pieces, ".");
        histogram_prefix_pieces.pop_back();
        return name;
      };

  base::UmaHistogramCounts100000(make_histogram_name("TotalUrlSize"),
                                 url_request.url().spec().size());

  // HTTP/2 and HTTP/3 requests don't separate request line from headers so no
  // need to record header metrics separately.
  if (!IsMultiplexedConnection(response_info)) {
    base::UmaHistogramCounts100000(make_histogram_name("TotalHeadersSize"),
                                   raw_request_headers_size);
  }

  // For HTTP/2 and HTTP/3 the request line is included in the headers, but
  // `raw_request_line_size_` is 0 for these requests, so we can add it
  // unconditionally for all requests.
  size_t total_request_size = raw_request_headers_size + raw_request_line_size;
  base::UmaHistogramCounts100000(make_histogram_name("TotalRequestSize"),
                                 total_request_size);
}

void MaybeRecordSharedDictionaryUsedResponseMetrics(
    int error_code,
    network::mojom::RequestDestination destination,
    const net::HttpResponseInfo& response_info,
    bool shared_dictionary_allowed_check_passed) {
  if (response_info.was_cached) {
    return;
  }
  if (response_info.did_use_shared_dictionary) {
    base::UmaHistogramSparse(
        base::StrCat({"Net.SharedDictionaryUsedResponseErrorCodes2.",
                      GetDestinationTypePartString(destination), ".",
                      GetCertStatePartString(response_info.ssl_info)}),
        -error_code);
  }

  if (shared_dictionary_allowed_check_passed &&
      destination == network::mojom::RequestDestination::kDocument) {
    base::UmaHistogramBoolean(
        base::StrCat(
            {"Net.SharedDictionaryUsedByResponseWhenAvailable2.MainFrame.",
             net::HttpConnectionInfoCoarseToString(
                 net::HttpConnectionInfoToCoarse(
                     response_info.connection_info)),
             ".", GetCertStatePartString(response_info.ssl_info)}),
        response_info.did_use_shared_dictionary);
  }
}

}  // namespace network::url_loader_util
