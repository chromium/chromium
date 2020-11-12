// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/shared_resources_data_source.h"

#include <stddef.h>
#include <string>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/resources/media/grit/media_internals_resources.h"
#include "content/browser/resources/media/grit/media_internals_resources_map.h"
#include "content/grit/content_resources.h"
#include "content/grit/content_resources_map.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"
#include "mojo/public/js/grit/mojo_bindings_resources_map.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "skia/grit/skia_resources.h"
#include "skia/grit/skia_resources_map.h"
#include "ui/base/layout.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_generated_resources_map.h"
#include "ui/resources/grit/webui_resources_map.h"

#if defined(OS_CHROMEOS)
#include "chromeos/grit/chromeos_resources.h"
#include "chromeos/grit/chromeos_resources_map.h"
#endif

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace content {

namespace {

using ResourcesMap = std::unordered_map<std::string, int>;

const std::map<std::string, std::string> CreatePathPrefixAliasesMap() {
  // TODO(rkc): Once we have a separate source for apps, remove '*/apps/'
  // aliases.
  std::map<std::string, std::string> aliases = {
    {"../../views/resources/default_100_percent/common/", "images/apps/"},
    {"../../views/resources/default_200_percent/common/", "images/2x/apps/"},
    {"@out_folder@/gen/ui/webui/resources/", ""},
#if defined(OS_ANDROID)
    // This is a temporary fix for `target_cpu = "arm64"`. See the bug for
    // more context: crbug.com/1020284.
    {"@out_folder@/android_clang_arm/gen/ui/webui/resources/", ""},
#endif  // defined(OS_ANDROID)
#if defined(OS_CHROMEOS)
    {"@out_folder@/gen/ui/chromeos/", "chromeos/"},
#endif  // defined(OS_CHROMEOS)
  };

#if !defined(OS_ANDROID)
  aliases["../../../third_party/lottie/"] = "lottie/";
#endif  // !defined(OS_ANDROID)
  return aliases;
}

const std::map<int, std::string> CreateContentResourceIdToAliasMap() {
  return std::map<int, std::string>{
      {IDR_ORIGIN_MOJO_HTML, "mojo/url/mojom/origin.mojom.html"},
      {IDR_ORIGIN_MOJO_JS, "mojo/url/mojom/origin.mojom-lite.js"},
      {IDR_UNGUESSABLE_TOKEN_MOJO_HTML,
       "mojo/mojo/public/mojom/base/unguessable_token.mojom.html"},
      {IDR_UNGUESSABLE_TOKEN_MOJO_JS,
       "mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js"},
      {IDR_URL_MOJO_HTML, "mojo/url/mojom/url.mojom.html"},
      {IDR_URL_MOJO_JS, "mojo/url/mojom/url.mojom-lite.js"},
      {IDR_URL_MOJOM_WEBUI_JS, "mojo/url/mojom/url.mojom-webui.js"},
      {IDR_VULKAN_INFO_MOJO_JS, "gpu/ipc/common/vulkan_info.mojom-lite.js"},
      {IDR_VULKAN_TYPES_MOJO_JS, "gpu/ipc/common/vulkan_types.mojom-lite.js"},
  };
}

const std::map<int, std::string> CreateSkiaResourceIdToAliasMap() {
  return std::map<int, std::string>{
      {IDR_SKIA_BITMAP_MOJOM_LITE_JS,
       "mojo/skia/public/mojom/bitmap.mojom-lite.js"},
      {IDR_SKIA_IMAGE_INFO_MOJOM_LITE_JS,
       "mojo/skia/public/mojom/image_info.mojom-lite.js"},
      {IDR_SKIA_SKCOLOR_MOJOM_LITE_JS,
       "mojo/skia/public/mojom/skcolor.mojom-lite.js"},
  };
}

#if defined(OS_CHROMEOS)
const std::map<int, std::string> CreateChromeosMojoResourceIdToAliasMap() {
  return std::map<int, std::string>{
      {IDR_CELLULAR_SETUP_MOJOM_HTML,
       "mojo/chromeos/services/cellular_setup/public/mojom/"
       "cellular_setup.mojom.html"},
      {IDR_CELLULAR_SETUP_MOJOM_LITE_JS,
       "mojo/chromeos/services/cellular_setup/public/mojom/"
       "cellular_setup.mojom-lite.js"},
      {IDR_MULTIDEVICE_DEVICE_SYNC_MOJOM_HTML,
       "mojo/chromeos/services/device_sync/public/mojom/"
       "device_sync.mojom.html"},
      {IDR_MULTIDEVICE_DEVICE_SYNC_MOJOM_LITE_JS,
       "mojo/chromeos/services/device_sync/public/mojom/"
       "device_sync.mojom-lite.js"},
      {IDR_MULTIDEVICE_MULTIDEVICE_SETUP_MOJOM_HTML,
       "mojo/chromeos/services/multidevice_setup/public/mojom/"
       "multidevice_setup.mojom.html"},
      {IDR_MULTIDEVICE_MULTIDEVICE_SETUP_MOJOM_LITE_JS,
       "mojo/chromeos/services/multidevice_setup/public/mojom/"
       "multidevice_setup.mojom-lite.js"},
      {IDR_MULTIDEVICE_MULTIDEVICE_TYPES_MOJOM_HTML,
       "mojo/chromeos/components/multidevice/mojom/"
       "multidevice_types.mojom.html"},
      {IDR_MULTIDEVICE_MULTIDEVICE_TYPES_MOJOM_LITE_JS,
       "mojo/chromeos/components/multidevice/mojom/"
       "multidevice_types.mojom-lite.js"},
      {IDR_NETWORK_CONFIG_MOJOM_HTML,
       "mojo/chromeos/services/network_config/public/mojom/"
       "cros_network_config.mojom.html"},
      {IDR_NETWORK_CONFIG_MOJOM_LITE_JS,
       "mojo/chromeos/services/network_config/public/mojom/"
       "cros_network_config.mojom-lite.js"},
      {IDR_NETWORK_CONFIG_TYPES_MOJOM_HTML,
       "mojo/chromeos/services/network_config/public/mojom/"
       "network_types.mojom.html"},
      {IDR_NETWORK_CONFIG_TYPES_MOJOM_LITE_JS,
       "mojo/chromeos/services/network_config/public/mojom/"
       "network_types.mojom-lite.js"},
      {IDR_IP_ADDRESS_MOJOM_HTML,
       "mojo/services/network/public/mojom/"
       "ip_address.mojom.html"},
      {IDR_IP_ADDRESS_MOJOM_LITE_JS,
       "mojo/services/network/public/mojom/"
       "ip_address.mojom-lite.js"},
      {IDR_NETWORK_HEALTH_MOJOM_HTML,
       "mojo/chromeos/services/network_health/public/mojom/"
       "network_health.mojom.html"},
      {IDR_NETWORK_HEALTH_MOJOM_LITE_JS,
       "mojo/chromeos/services/network_health/public/mojom/"
       "network_health.mojom-lite.js"},
      {IDR_NETWORK_DIAGNOSTICS_MOJOM_HTML,
       "mojo/chromeos/services/network_health/public/mojom/"
       "network_diagnostics.mojom.html"},
      {IDR_NETWORK_DIAGNOSTICS_MOJOM_LITE_JS,
       "mojo/chromeos/services/network_health/public/mojom/"
       "network_diagnostics.mojom-lite.js"},
  };
}
#endif  // !defined(OS_CHROMEOS)

void AddResource(const std::string& path,
                 int resource_id,
                 ResourcesMap* resources_map) {
  if (!resources_map->insert(std::make_pair(path, resource_id)).second)
    NOTREACHED() << "Redefinition of '" << path << "'";
}

void AddResourcesToMap(ResourcesMap* resources_map) {
  const std::map<std::string, std::string> aliases =
      CreatePathPrefixAliasesMap();

  for (size_t i = 0; i < kWebuiResourcesSize; ++i) {
    const auto& resource = kWebuiResources[i];
    AddResource(resource.name, resource.value, resources_map);

    for (auto it = aliases.begin(); it != aliases.end(); ++it) {
      if (base::StartsWith(resource.name, it->first,
                           base::CompareCase::SENSITIVE)) {
        std::string resource_name(resource.name);
        AddResource(it->second + resource_name.substr(it->first.length()),
                    resource.value, resources_map);
      }
    }
  }
}

// Adds |resources| to |resources_map|, but renames each resource according to
// the scheme in |resource_aliases|, which maps from resource ID to resource
// alias. Note that resources which do not have an alias will not be added.
void AddAliasedResourcesToMap(
    const std::map<int, std::string>& resource_aliases,
    const GritResourceMap resources[],
    size_t resources_size,
    ResourcesMap* resources_map) {
  for (size_t i = 0; i < resources_size; ++i) {
    const auto& resource = resources[i];

    const auto it = resource_aliases.find(resource.value);
    if (it == resource_aliases.end())
      continue;

    AddResource(it->second, resource.value, resources_map);
  }
}

// Adds |resources| to |resources_map| using the path given by resource_path in
// each GRD entry.
void AddGritResourcesToMap(base::span<const GritResourceMap> resources,
                           ResourcesMap* resources_map) {
  for (const GritResourceMap& entry : resources)
    AddResource(entry.name, entry.value, resources_map);
}

const ResourcesMap* CreateResourcesMap() {
  ResourcesMap* result = new ResourcesMap();
  AddResourcesToMap(result);
  AddAliasedResourcesToMap(CreateContentResourceIdToAliasMap(),
                           kContentResources, kContentResourcesSize, result);
  AddAliasedResourcesToMap(CreateContentResourceIdToAliasMap(),
                           kMediaInternalsResources,
                           kMediaInternalsResourcesSize, result);
  AddGritResourcesToMap(
      base::make_span(kWebuiGeneratedResources, kWebuiGeneratedResourcesSize),
      result);
  AddGritResourcesToMap(
      base::make_span(kMojoBindingsResources, kMojoBindingsResourcesSize),
      result);
  AddAliasedResourcesToMap(CreateSkiaResourceIdToAliasMap(), kSkiaResources,
                           kSkiaResourcesSize, result);
#if defined(OS_CHROMEOS)
  AddAliasedResourcesToMap(CreateChromeosMojoResourceIdToAliasMap(),
                           kChromeosResources, kChromeosResourcesSize, result);
#endif  // !defined(OS_CHROMEOS)
  return result;
}

const ResourcesMap& GetResourcesMap() {
  // This pointer will be intentionally leaked on shutdown.
  static const ResourcesMap* resources_map = CreateResourcesMap();
  return *resources_map;
}

int GetIdrForPath(const std::string& path) {
  const ResourcesMap& resources_map = GetResourcesMap();
  auto it = resources_map.find(path);
  return it != resources_map.end() ? it->second : -1;
}

}  // namespace

// static
std::unique_ptr<SharedResourcesDataSource>
SharedResourcesDataSource::CreateForChromeScheme() {
  return std::make_unique<SharedResourcesDataSource>(PassKey(),
                                                     kChromeUIScheme);
}

// static
std::unique_ptr<SharedResourcesDataSource>
SharedResourcesDataSource::CreateForChromeUntrustedScheme() {
  return std::make_unique<SharedResourcesDataSource>(PassKey(),
                                                     kChromeUIUntrustedScheme);
}

SharedResourcesDataSource::SharedResourcesDataSource(PassKey,
                                                     const std::string& scheme)
    : scheme_(scheme) {}

SharedResourcesDataSource::~SharedResourcesDataSource() = default;

std::string SharedResourcesDataSource::GetSource() {
  // URLDataManagerBackend assumes that chrome:// data sources return just the
  // hostname for GetSource().
  if (scheme_ == kChromeUIScheme)
    return kChromeUIResourcesHost;

  // We only expect chrome-untrusted:// scheme at this point.
  DCHECK_EQ(kChromeUIUntrustedScheme, scheme_);

  // Other schemes (i.e. chrome-untrusted://) return the scheme and host
  // together.
  return base::StrCat(
      {scheme_, url::kStandardSchemeSeparator, kChromeUIResourcesHost});
}

void SharedResourcesDataSource::StartDataRequest(
    const GURL& url,
    const WebContents::Getter& wc_getter,
    URLDataSource::GotDataCallback callback) {
  const std::string path = URLDataSource::URLToRequestPath(url);
  int idr = GetIdrForPath(path);
  DCHECK_NE(-1, idr) << " path: " << path;
  scoped_refptr<base::RefCountedMemory> bytes;

  if (idr == IDR_WEBUI_CSS_TEXT_DEFAULTS_CSS) {
    std::string css = webui::GetWebUiCssTextDefaults();
    bytes = base::RefCountedString::TakeString(&css);
  } else if (idr == IDR_WEBUI_CSS_TEXT_DEFAULTS_MD_CSS) {
    std::string css = webui::GetWebUiCssTextDefaultsMd();
    bytes = base::RefCountedString::TakeString(&css);
  } else {
    bytes = GetContentClient()->GetDataResourceBytes(idr);
  }

  std::move(callback).Run(std::move(bytes));
}

bool SharedResourcesDataSource::AllowCaching() {
  // Should not be cached to reflect dynamically-generated contents that may
  // depend on the current locale.
  return false;
}

std::string SharedResourcesDataSource::GetMimeType(const std::string& path) {
  if (path.empty())
    return "text/html";

#if defined(OS_WIN)
  base::FilePath file(base::UTF8ToWide(path));
  std::string extension = base::WideToUTF8(file.FinalExtension());
#else
  base::FilePath file(path);
  std::string extension = file.FinalExtension();
#endif

  if (!extension.empty())
    extension.erase(0, 1);

  if (extension == "html")
    return "text/html";

  if (extension == "css")
    return "text/css";

  if (extension == "js")
    return "application/javascript";

  if (extension == "png")
    return "image/png";

  if (extension == "gif")
    return "image/gif";

  if (extension == "svg")
    return "image/svg+xml";

  if (extension == "woff2")
    return "application/font-woff2";

  NOTREACHED() << path;
  return "text/plain";
}

bool SharedResourcesDataSource::ShouldServeMimeTypeAsContentTypeHeader() {
  return true;
}

std::string SharedResourcesDataSource::GetAccessControlAllowOriginForOrigin(
    const std::string& origin) {
  // For now we give access only for origins with the allowed scheme.
  // According to CORS spec, Access-Control-Allow-Origin header doesn't support
  // wildcards, so we need to set its value explicitly by passing the |origin|
  // back.
  const std::string allowed_origin_prefix =
      base::StrCat({scheme_, url::kStandardSchemeSeparator});
  if (!base::StartsWith(origin, allowed_origin_prefix,
                        base::CompareCase::SENSITIVE)) {
    return "null";
  }
  return origin;
}

std::string SharedResourcesDataSource::GetContentSecurityPolicy(
    network::mojom::CSPDirectiveName directive) {
  if (directive == network::mojom::CSPDirectiveName::WorkerSrc) {
    return "worker-src blob: 'self';";
  } else if (directive ==
                 network::mojom::CSPDirectiveName::RequireTrustedTypesFor ||
             directive == network::mojom::CSPDirectiveName::TrustedTypes) {
    // TODO(crbug.com/1098690): Trusted Type Polymer
    // This removes require-trusted-types-for and trusted-types directives
    // from the CSP header.
    return std::string();
  }

  return content::URLDataSource::GetContentSecurityPolicy(directive);
}

}  // namespace content
