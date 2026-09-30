# IWA Public Interfaces (`components/webapps/isolated_web_apps/public`)

This directory contains public interfaces and header utilities for the Isolated
Web Apps component.

## Key Components

- `IwaRuntimeDataProvider` (`iwa_runtime_data_provider.{h,cc}`): Abstract
  interface providing access to runtime key rotation, managed allowlist,
  user-install allowlist, blocklist, and special permission data for IWAs.
- `iwa_entitlements.{h,cc}`: Defines `IwaEntitlement`, `IwaVersionRange`,
  `IwaEntitlementsSet`, and `GetEntitlementForFeature()` to map Permissions
  Policy features to required entitlements for user-installed IWAs.
- `header_utils.{h,cc}`: Helpers in namespace `web_app::iwa` for constructing
  default/dev-mode Content Security Policies (CSP) and applying required COOP,
  COEP, CORP, and CSP headers to `net::HttpResponseHeaders` and
  `network::mojom::ParsedHeaders`.
