# IWA BrowserContext Service Factory (`components/webapps/isolated_web_apps/service`)

This directory provides the base `BrowserContextKeyedServiceFactory` for
Isolated Web App keyed services.

## Key Components

- `IsolatedWebAppBrowserContextServiceFactory`
  (`isolated_web_app_browser_context_service_factory.{h,cc}`): Base class for
  IWA keyed service factories (such as `IsolatedWebAppReaderRegistryFactory`)
  that overrides `GetBrowserContextToUse()` to only instantiate services for
  `content::BrowserContext` instances where Isolated Web Apps are enabled via
  `content::AreIsolatedWebAppsEnabled()`.
