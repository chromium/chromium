# IWA URL Loading (`components/webapps/isolated_web_apps/url_loading`)

This directory implements the `isolated-app://` protocol handler and
`network::mojom::URLLoaderFactory` for Isolated Web Apps.

## Key Components

- `IsolatedWebAppURLLoaderFactory` (`url_loader_factory.{h,cc}`): Self-deleting
  `network::mojom::URLLoaderFactory` that intercepts `isolated-app://` requests,
  injects required COOP/COEP/CORP/CSP headers (and re-parses headers for Service
  Worker scripts), queries `IwaClient::GetIwaSourceForRequest()`, and dispatches
  requests to `IsolatedWebAppURLLoader`, `HandleProxy()`, or a synthetic
  `GeneratedResponse`.
- `IsolatedWebAppURLLoader` (`url_loader.{h,cc}`): Self-owned
  `network::mojom::URLLoader` that reads HTTP responses and streams response
  bodies from a Signed Web Bundle via `IsolatedWebAppReaderRegistry`.
- `utils.{h,cc}`: Helpers for completing requests with synthetic HTTP responses
  (`CompleteWithGeneratedResponse()`), logging errors to DevTools console and
  failing requests (`LogErrorMessageToConsole()`, `LogErrorAndFail()`), and
  forwarding developer-mode proxy requests (`HandleProxy()`).
- `header_utils.{h,cc}`: Helpers in namespace `web_app::iwa` for constructing
  default/dev-mode Content Security Policies (CSP) and applying required COOP,
  COEP, CORP, and CSP headers to `net::HttpResponseHeaders` and
  `network::mojom::ParsedHeaders`.
