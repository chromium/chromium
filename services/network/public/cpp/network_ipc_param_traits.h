// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_

#include <string>

#include "base/component_export.h"
#include "base/pickle.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/param_traits_macros.h"
#include "net/base/auth.h"
#include "net/base/ip_endpoint.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_version.h"
#include "net/nqe/effective_connection_type.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/net_ipc_param_traits.h"
#include "services/network/public/cpp/origin_policy.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/blocked_by_response_reason.mojom-shared.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "url/ipc/url_param_traits.h"
#include "url/origin.h"

// This file defines IPC::ParamTraits for network:: classes / structs.
// For IPC::ParamTraits for net:: class / structs, see net_ipc_param_traits.h.

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::CorsError,
                          network::mojom::CorsError::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::CredentialsMode,
                          network::mojom::CredentialsMode::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::RedirectMode,
                          network::mojom::RedirectMode::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::RequestMode,
                          network::mojom::RequestMode::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::CorsPreflightPolicy,
                          network::mojom::CorsPreflightPolicy::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::BlockedByResponseReason,
                          network::mojom::BlockedByResponseReason::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::IPAddressSpace,
                          network::mojom::IPAddressSpace::kMaxValue)

IPC_STRUCT_TRAITS_BEGIN(network::CorsErrorStatus)
  IPC_STRUCT_TRAITS_MEMBER(cors_error)
  IPC_STRUCT_TRAITS_MEMBER(failed_parameter)
  IPC_STRUCT_TRAITS_MEMBER(resource_address_space)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(network::URLLoaderCompletionStatus)
  IPC_STRUCT_TRAITS_MEMBER(error_code)
  IPC_STRUCT_TRAITS_MEMBER(extended_error_code)
  IPC_STRUCT_TRAITS_MEMBER(exists_in_cache)
  IPC_STRUCT_TRAITS_MEMBER(completion_time)
  IPC_STRUCT_TRAITS_MEMBER(encoded_data_length)
  IPC_STRUCT_TRAITS_MEMBER(encoded_body_length)
  IPC_STRUCT_TRAITS_MEMBER(decoded_body_length)
  IPC_STRUCT_TRAITS_MEMBER(cors_error_status)
  IPC_STRUCT_TRAITS_MEMBER(trust_token_operation_status)
  IPC_STRUCT_TRAITS_MEMBER(ssl_info)
  IPC_STRUCT_TRAITS_MEMBER(blocked_by_response_reason)
  IPC_STRUCT_TRAITS_MEMBER(should_report_corb_blocking)
  IPC_STRUCT_TRAITS_MEMBER(proxy_server)
  IPC_STRUCT_TRAITS_MEMBER(resolve_error_info)

IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::FetchResponseType,
                          network::mojom::FetchResponseType::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::TrustTokenOperationStatus,
                          network::mojom::TrustTokenOperationStatus::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::OriginPolicyState,
                          network::OriginPolicyState::kMaxValue)

IPC_STRUCT_TRAITS_BEGIN(network::OriginPolicyContents)
  IPC_STRUCT_TRAITS_MEMBER(ids)
  IPC_STRUCT_TRAITS_MEMBER(feature_policy)
  IPC_STRUCT_TRAITS_MEMBER(content_security_policies)
  IPC_STRUCT_TRAITS_MEMBER(content_security_policies_report_only)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(network::OriginPolicy)
  IPC_STRUCT_TRAITS_MEMBER(state)
  IPC_STRUCT_TRAITS_MEMBER(policy_url)
  IPC_STRUCT_TRAITS_MEMBER(contents)
IPC_STRUCT_TRAITS_END()

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_
