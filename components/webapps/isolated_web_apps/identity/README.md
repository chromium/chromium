# IWA Identity Validation (`components/webapps/isolated_web_apps/identity`)

This directory validates the cryptographic identity of Signed Web Bundles
against their `SignedWebBundleId` and active key rotation rules.

## Key Components

- `IwaIdentityValidator` (`iwa_identity_validator.{h,cc}`): Singleton validator
  implementing `web_package::IdentityValidator` that checks whether the public
  keys in an Integrity Block match the expected `SignedWebBundleId` or a rotated
  key (including optional soft-rotation `previous_key` support) supplied by
  `IwaRuntimeDataProvider`.
