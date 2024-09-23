// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/network_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "net/base/data_url.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "url/gurl.h"

namespace {

net::registry_controlled_domains::PrivateRegistryFilter
getNetPrivateRegistryFilter(
    blink::network_utils::PrivateRegistryFilter filter) {
  switch (filter) {
    case blink::network_utils::kIncludePrivateRegistries:
      return net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES;
    case blink::network_utils::kExcludePrivateRegistries:
      return net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES;
  }
  // There are only two network_utils::PrivateRegistryFilter enum entries, so
  // we should never reach this point. However, we must have a default return
  // value to avoid a compiler error.
  NOTREACHED_IN_MIGRATION();
  return net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES;
}

}  // namespace

namespace blink {

namespace network_utils {

bool IsReservedIPAddress(const StringView& host) {
  net::IPAddress address;
  StringUTF8Adaptor utf8(host);
  if (!net::ParseURLHostnameToAddress(utf8.AsStringView(), &address)) {
    return false;
  }
  return !address.IsPubliclyRoutable();
}

String GetDomainAndRegistry(const StringView& host,
                            PrivateRegistryFilter filter) {
  StringUTF8Adaptor host_utf8(host);
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      host_utf8.AsStringView(), getNetPrivateRegistryFilter(filter));
  return String(domain.data(), domain.length());
}

std::tuple<int, ResourceResponse, scoped_refptr<SharedBuffer>> ParseDataURL(
    const KURL& url,
    const String& method,
    ukm::SourceId source_id,
    ukm::UkmRecorder* recorder) {
  base::ElapsedTimer timer;

  std::string utf8_mime_type;
  std::string utf8_charset;
  std::string data_string;
  scoped_refptr<net::HttpResponseHeaders> headers;

  net::Error result =
      net::DataURL::BuildResponse(GURL(url), method.Ascii(), &utf8_mime_type,
                                  &utf8_charset, &data_string, &headers);
  if (result != net::OK)
    return std::make_tuple(result, ResourceResponse(), nullptr);

  auto buffer = SharedBuffer::Create(data_string.data(), data_string.size());
  // The below code is the same as in
  // `CreateResourceForTransparentPlaceholderImage()`.
  ResourceResponse response;
  response.SetHttpStatusCode(200);
  response.SetHttpStatusText(AtomicString("OK"));
  response.SetCurrentRequestUrl(url);
  response.SetMimeType(WebString::FromUTF8(utf8_mime_type));
  response.SetExpectedContentLength(buffer->size());
  response.SetTextEncodingName(WebString::FromUTF8(utf8_charset));

  size_t iter = 0;
  std::string name;
  std::string value;
  while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
    response.AddHttpHeaderField(WebString::FromLatin1(name),
                                WebString::FromLatin1(value));
  }

  base::TimeDelta elapsed = timer.Elapsed();
  base::UmaHistogramMicrosecondsTimes("Blink.Network.ParseDataURLTime",
                                      elapsed);
  size_t length = url.GetString().length();
  base::UmaHistogramCounts10M("Blink.Network.DataUrlLength",
                              static_cast<int>(length));
  if (length >= 0 && length < 1000) {
    base::UmaHistogramMicrosecondsTimes(
        "Blink.Network.ParseDataURLTime.Under1000Char", elapsed);
  } else if (length >= 1000 && length < 100000) {
    base::UmaHistogramMicrosecondsTimes(
        "Blink.Network.ParseDataURLTime.Under100000Char", elapsed);
  } else {
    base::UmaHistogramMicrosecondsTimes(
        "Blink.Network.ParseDataURLTime.Over100000Char", elapsed);
  }
  bool is_image = utf8_mime_type.starts_with("image/");
  if (is_image) {
    base::UmaHistogramCounts10M("Blink.Network.DataUrlLength.Image",
                                static_cast<int>(length));
    base::UmaHistogramMicrosecondsTimes("Blink.Network.ParseDataURLTime.Image",
                                        elapsed);
    if (length >= 0 && length < 1000) {
      base::UmaHistogramMicrosecondsTimes(
          "Blink.Network.ParseDataURLTime.Image.Under1000Char", elapsed);
    } else if (length >= 1000 && length < 100000) {
      base::UmaHistogramMicrosecondsTimes(
          "Blink.Network.ParseDataURLTime.Image.Under100000Char", elapsed);
    } else {
      base::UmaHistogramMicrosecondsTimes(
          "Blink.Network.ParseDataURLTime.Image.Over100000Char", elapsed);
    }
  }
  if (source_id != ukm::kInvalidSourceId && recorder) {
    ukm::builders::Network_DataUrls builder(source_id);
    builder.SetUrlLength(ukm::GetExponentialBucketMinForCounts1000(length));
    builder.SetParseTime(elapsed.InMicroseconds());
    builder.SetIsImage(is_image);
    builder.Record(recorder);
  }

  return std::make_tuple(net::OK, std::move(response), std::move(buffer));
}

bool IsDataURLMimeTypeSupported(const KURL& url,
                                std::string* data,
                                std::string* mime_type) {
  std::string utf8_mime_type;
  std::string utf8_charset;
  if (!net::DataURL::Parse(GURL(url), &utf8_mime_type, &utf8_charset, data))
    return false;
  if (!blink::IsSupportedMimeType(utf8_mime_type))
    return false;
  if (mime_type)
    utf8_mime_type.swap(*mime_type);
  return true;
}

bool IsRedirectResponseCode(int response_code) {
  return net::HttpResponseHeaders::IsRedirectResponseCode(response_code);
}

bool IsCertificateTransparencyRequiredError(int error_code) {
  return error_code == net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED;
}

String GenerateAcceptLanguageHeader(const String& lang) {
  return WebString::FromUTF8(
      net::HttpUtil::GenerateAcceptLanguageHeader(lang.Utf8()));
}

String ExpandLanguageList(const String& lang) {
  return WebString::FromUTF8(net::HttpUtil::ExpandLanguageList(lang.Utf8()));
}

Vector<char> ParseMultipartBoundary(const AtomicString& content_type_header) {
  std::string utf8_string = content_type_header.Utf8();
  std::string mime_type;
  std::string charset;
  bool had_charset = false;
  std::string boundary;
  net::HttpUtil::ParseContentType(utf8_string, &mime_type, &charset,
                                  &had_charset, &boundary);
  base::TrimString(boundary, " \"", &boundary);
  Vector<char> result;
  result.AppendSpan(base::span(boundary));
  return result;
}

}  // namespace network_utils

}  // namespace blink
