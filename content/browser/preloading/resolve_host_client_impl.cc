// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/resolve_host_client_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/types/optional_ref.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/request_priority.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace content {

ResolveHostClientImpl::ResolveHostClientImpl(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    base::optional_ref<const base::UnguessableToken> network_restrictions_id,
    ResolveHostCallback callback,
    network::mojom::NetworkContext* network_context,
    net::RequestPriority initial_priority)
    : callback_(std::move(callback)) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M159);

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->initial_priority = initial_priority;
  parameters->is_speculative = true;
  parameters->purpose =
      network::mojom::ResolveHostParameters::Purpose::kPreconnect;
  parameters->network_restrictions_id =
      network_restrictions_id.CopyAsOptional();
  // Intentionally using a SchemeHostPort. Resolving http:// scheme host will
  // fail when a HTTPS resource record exists due to DNS-based scheme upgrade
  // functionality.
  network_context->ResolveHost(
      network::mojom::HostResolverHost::NewSchemeHostPort(
          url::SchemeHostPort(url)),
      network_anonymization_key, std::move(parameters),
      receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(base::BindOnce(
      &ResolveHostClientImpl::OnConnectionError, base::Unretained(this)));
}

ResolveHostClientImpl::~ResolveHostClientImpl() = default;

void ResolveHostClientImpl::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const net::AddressList& resolved_addresses,
    const net::HostResolverEndpointResults& alternative_endpoints) {
  std::move(callback_).Run(result == net::OK);
}

void ResolveHostClientImpl::OnConnectionError() {
  std::move(callback_).Run(false);
}

}  // namespace content
