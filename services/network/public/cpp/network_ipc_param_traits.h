// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_

#include "base/component_export.h"
#include "base/pickle.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/param_traits_macros.h"
#include "net/base/auth.h"
#include "net/base/ip_endpoint.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
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
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "url/ipc/url_param_traits.h"
#include "url/origin.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT COMPONENT_EXPORT(NETWORK_CPP_BASE)

// This file defines IPC::ParamTraits for network:: classes / structs.
// For IPC::ParamTraits for net:: class / structs, see net_ipc_param_traits.h.

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::CredentialsMode,
                          network::mojom::CredentialsMode::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::RedirectMode,
                          network::mojom::RedirectMode::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::RequestMode,
                          network::mojom::RequestMode::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::FetchResponseType,
                          network::mojom::FetchResponseType::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::TrustTokenOperationStatus,
                          network::mojom::TrustTokenOperationStatus::kMaxValue)

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_
