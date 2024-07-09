// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/loader/cors/cors_error_string.h"

#include <initializer_list>

#include "base/numerics/safe_conversions.h"
#include "services/network/public/mojom/cors.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

namespace cors {

namespace {

void Append(StringBuilder& builder, std::initializer_list<StringView> views) {
  for (const StringView& view : views) {
    builder.Append(view);
  }
}

bool IsPreflightError(network::mojom::CorsError error_code) {
  switch (error_code) {
    case network::mojom::CorsError::kPreflightWildcardOriginNotAllowed:
    case network::mojom::CorsError::kPreflightMissingAllowOriginHeader:
    case network::mojom::CorsError::kPreflightMultipleAllowOriginValues:
    case network::mojom::CorsError::kPreflightInvalidAllowOriginValue:
    case network::mojom::CorsError::kPreflightAllowOriginMismatch:
    case network::mojom::CorsError::kPreflightInvalidAllowCredentials:
    case network::mojom::CorsError::kPreflightInvalidStatus:
    case network::mojom::CorsError::kPreflightDisallowedRedirect:
    case network::mojom::CorsError::kPreflightMissingAllowPrivateNetwork:
    case network::mojom::CorsError::kPreflightInvalidAllowPrivateNetwork:
      return true;
    default:
      return false;
  }
}

StringView ShortAddressSpace(network::mojom::IPAddressSpace space) {
  switch (space) {
    case network::mojom::IPAddressSpace::kUnknown:
      return "unknown";
    case network::mojom::IPAddressSpace::kPublic:
      return "public";
    case network::mojom::IPAddressSpace::kPrivate:
      return "private";
    case network::mojom::IPAddressSpace::kLocal:
      return "local";
  }

  NOTREACHED_IN_MIGRATION() << "Invalid IPAddressSpace enum value: " << space;
  return "invalid";
}

}  // namespace

String GetErrorString(const network::CorsErrorStatus& status,
                      const KURL& initial_request_url,
                      const KURL& last_request_url,
                      const SecurityOrigin& origin,
                      ResourceType resource_type,
                      const AtomicString& initiator_name) {
  StringBuilder builder;
  static constexpr char kNoCorsInformation[] =
      " Have the server send the header with a valid value, or, if an opaque "
      "response serves your needs, set the request's mode to 'no-cors' to "
      "fetch the resource with CORS disabled.";

  using CorsError = network::mojom::CorsError;
  const StringView hint(
      status.failed_parameter.data(),
      base::checked_cast<wtf_size_t>(status.failed_parameter.size()));

  const char* resource_kind_raw =
      Resource::ResourceTypeToString(resource_type, initiator_name);
  String resource_kind(resource_kind_raw);
  if (strlen(resource_kind_raw) >= 2 && IsASCIILower(resource_kind_raw[1]))
    resource_kind = resource_kind.LowerASCII();

  Append(builder, {"Access to ", resource_kind, " at '",
                   last_request_url.GetString(), "' "});
  if (initial_request_url != last_request_url) {
    Append(builder,
           {"(redirected from '", initial_request_url.GetString(), "') "});
  }
  Append(builder, {"from origin '", origin.ToString(),
                   "' has been blocked by CORS policy: "});

  if (IsPreflightError(status.cors_error)) {
    builder.Append(
        "Response to preflight request doesn't pass access control check: ");
  }

  switch (status.cors_error) {
    case CorsError::kDisallowedByMode:
      builder.Append("Cross origin requests are not allowed by request mode.");
      break;
    case CorsError::kInvalidResponse:
      builder.Append("The response is invalid.");
      break;
    case CorsError::kInsecurePrivateNetwork:
      Append(builder, {"The request client is not a secure context and the "
                       "resource is in more-private address space `",
                       ShortAddressSpace(status.resource_address_space), "`."});
      break;
    case CorsError::kWildcardOriginNotAllowed:
    case CorsError::kPreflightWildcardOriginNotAllowed:
      builder.Append(
          "The value of the 'Access-Control-Allow-Origin' header in the "
          "response must not be the wildcard '*' when the request's "
          "credentials mode is 'include'.");
      if (initiator_name == fetch_initiator_type_names::kXmlhttprequest) {
        builder.Append(
            " The credentials mode of requests initiated by the "
            "XMLHttpRequest is controlled by the withCredentials attribute.");
      }
      break;
    case CorsError::kMissingAllowOriginHeader:
    case CorsError::kPreflightMissingAllowOriginHeader:
      builder.Append(
          "No 'Access-Control-Allow-Origin' header is present on the "
          "requested resource.");
      if (initiator_name == fetch_initiator_type_names::kFetch) {
        builder.Append(
            " If an opaque response serves your needs, set the request's "
            "mode to 'no-cors' to fetch the resource with CORS disabled.");
      }
      break;
    case CorsError::kMultipleAllowOriginValues:
    case CorsError::kPreflightMultipleAllowOriginValues:
      Append(builder,
             {"The 'Access-Control-Allow-Origin' header contains multiple "
              "values '",
              hint, "', but only one is allowed."});
      if (initiator_name == fetch_initiator_type_names::kFetch)
        builder.Append(kNoCorsInformation);
      break;
    case CorsError::kInvalidAllowOriginValue:
    case CorsError::kPreflightInvalidAllowOriginValue:
      Append(builder, {"The 'Access-Control-Allow-Origin' header contains the "
                       "invalid value '",
                       hint, "'."});
      if (initiator_name == fetch_initiator_type_names::kFetch)
        builder.Append(kNoCorsInformation);
      break;
    case CorsError::kAllowOriginMismatch:
    case CorsError::kPreflightAllowOriginMismatch:
      Append(builder, {"The 'Access-Control-Allow-Origin' header has a value '",
                       hint, "' that is not equal to the supplied origin."});
      if (initiator_name == fetch_initiator_type_names::kFetch)
        builder.Append(kNoCorsInformation);
      break;
    case CorsError::kInvalidAllowCredentials:
    case CorsError::kPreflightInvalidAllowCredentials:
      Append(builder,
             {"The value of the 'Access-Control-Allow-Credentials' header in "
              "the response is '",
              hint,
              "' which must be 'true' when the request's credentials mode is "
              "'include'."});
      if (initiator_name == fetch_initiator_type_names::kXmlhttprequest) {
        builder.Append(
            " The credentials mode of requests initiated by the "
            "XMLHttpRequest is controlled by the withCredentials "
            "attribute.");
      }
      break;
    case CorsError::kCorsDisabledScheme:
      Append(builder,
             {"Cross origin requests are only supported for protocol schemes: ",
              SchemeRegistry::ListOfCorsEnabledURLSchemes(), "."});
      break;
    case CorsError::kPreflightInvalidStatus:
      builder.Append("It does not have HTTP ok status.");
      break;
    case CorsError::kPreflightDisallowedRedirect:
      builder.Append("Redirect is not allowed for a preflight request.");
      break;
    case CorsError::kPreflightMissingAllowPrivateNetwork:
      Append(builder, {"No 'Access-Control-Allow-Private-Network' header "
                       "was present in the preflight response for this private "
                       "network request targeting the `",
                       ShortAddressSpace(status.target_address_space),
                       "` address space."});
      break;
    case CorsError::kPreflightInvalidAllowPrivateNetwork:
      Append(builder,
             {"The 'Access-Control-Allow-Private-Network' header in the "
              "preflight response for this private network request targeting "
              "the `",
              ShortAddressSpace(status.target_address_space),
              "` address space had a value of '", hint, "',  not 'true'."});
      break;
    case CorsError::kInvalidAllowMethodsPreflightResponse:
      builder.Append(
          "Cannot parse Access-Control-Allow-Methods response header field in "
          "preflight response.");
      break;
    case CorsError::kInvalidAllowHeadersPreflightResponse:
      builder.Append(
          "Cannot parse Access-Control-Allow-Headers response header field in "
          "preflight response.");
      break;
    case CorsError::kMethodDisallowedByPreflightResponse:
      Append(builder, {"Method ", hint,
                       " is not allowed by Access-Control-Allow-Methods in "
                       "preflight response."});
      break;
    case CorsError::kHeaderDisallowedByPreflightResponse:
      Append(builder, {"Request header field ", hint,
                       " is not allowed by "
                       "Access-Control-Allow-Headers in preflight response."});
      break;
    case CorsError::kRedirectContainsCredentials:
      Append(builder, {"Redirect location '", hint,
                       "' contains a username and password, which is "
                       "disallowed for cross-origin requests."});
      break;
    case CorsError::kInvalidPrivateNetworkAccess:
      Append(builder, {"Request had a target IP address space of `",
                       ShortAddressSpace(status.target_address_space),
                       "` yet the resource is in address space `",
                       ShortAddressSpace(status.resource_address_space), "`."});
      break;
    case CorsError::kUnexpectedPrivateNetworkAccess:
      Append(builder, {"Request had no target IP address space, yet the "
                       "resource is in address space `",
                       ShortAddressSpace(status.resource_address_space), "`."});
      break;
    case CorsError::kPreflightMissingPrivateNetworkAccessId:
      Append(
          builder,
          {"No 'Private-Network-Access-Id' header was present in the "
           "preflight response for this private network request targeting "
           "the `",
           ShortAddressSpace(status.target_address_space), "` address space."});
      break;
    case CorsError::kPreflightMissingPrivateNetworkAccessName:
      Append(
          builder,
          {"No 'Private-Network-Access-Name' header was present in the "
           "preflight response for this private network request targeting "
           "the `",
           ShortAddressSpace(status.target_address_space), "` address space."});
      break;
    case CorsError::kPrivateNetworkAccessPermissionUnavailable:
      Append(builder, {"Unable to ask for permission to access the `",
                       ShortAddressSpace(status.target_address_space),
                       "` IP address space."});
      break;
    case CorsError::kPrivateNetworkAccessPermissionDenied:
      Append(builder, {"Permission was denied for this request to access the `",
                       ShortAddressSpace(status.target_address_space),
                       "` address space."});
  }
  return builder.ToString();
}

}  // namespace cors

}  // namespace blink
