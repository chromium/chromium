# [Web Apps](../../README.md) - Manifest representations in code

This is a list of all the places where we represent
[manifest](https://w3c.github.io/manifest/) data in our codebase.

 - [blink.mojom.Manifest](../../../../third_party/blink/public/mojom/manifest/manifest.mojom)\
   Mojo IPC representation between Blink and the browser.
   Output of the [Blink manifest parser](../../../../third_party/blink/renderer/modules/manifest/manifest_parser.cc).

 - [blink::Manifest](../../../../third_party/blink/public/common/manifest/manifest.h)\
   Contains subtypes representing certain fields of the manifest that need to be duplicated in handwritten C++ for assorted reasons.
   Used to be a full duplicate manifest definition until https://crbug.com/1233362.

 - [WebAppInstallInfo](../web_app_install_info.h)\
   Used for installation and updates.

 - [web_app::WebApp](../web_app.h)\
   Installed web app representation in RAM.

 - [web_app.WebAppProto](../proto/web_app.proto)\
   Installed web app representation on disk.

 - [sync_pb.WebAppSpecificsProto](../../../../components/sync/protocol/web_app_specifics.proto)\
   Installed web app representation in sync cloud.

 - [webapps.mojom.WebPageMetadata](../../../../components/webapps/common/web_page_metadata.mojom)\
   Manifest data provided by an HTML document.

 - [web_app::ParseOfflineManifest()](../preinstalled_web_app_utils.cc)\
   Custom JSON + PNG format for bundling WebAppInstallInfo data on disk for offline default web app installation.

 - [WebApkInfo](../../android/webapk/webapk_info.h)\
   Web app installation data that was packaged in an APK.

 - [payments::WebAppInstallationInfo](../../../../components/payments/content/web_app_manifest.h)\
   Payments code doesn't live under /chrome/browser, they have their own parser and representation.

 - [apps.proto.AppProvisioningResponse](../../apps/app_preload_service/proto/app_provisioning.proto)\
   Apps Preload Service server communication proto containing default web app install data.
