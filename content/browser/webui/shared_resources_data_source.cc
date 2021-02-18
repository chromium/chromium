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
#include "build/chromeos_buildflags.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/grit/chromeos_resources.h"
#include "chromeos/grit/chromeos_resources_map.h"
#endif

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace content {

namespace {

using ResourcesMap = std::unordered_map<std::string, int>;

const std::set<int> GetContentResourceIds() {
  return std::set<int>{
      IDR_ORIGIN_MOJO_HTML,
      IDR_ORIGIN_MOJO_JS,
      IDR_ORIGIN_MOJO_WEBUI_JS,
      IDR_UNGUESSABLE_TOKEN_MOJO_HTML,
      IDR_UNGUESSABLE_TOKEN_MOJO_JS,
      IDR_URL_MOJO_HTML,
      IDR_URL_MOJO_JS,
      IDR_URL_MOJOM_WEBUI_JS,
      IDR_VULKAN_INFO_MOJO_JS,
      IDR_VULKAN_TYPES_MOJO_JS,
  };
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
const std::set<int> GetChromeosMojoResourceIds() {
  return std::set<int>{
      IDR_CELLULAR_SETUP_MOJOM_HTML,
      IDR_CELLULAR_SETUP_MOJOM_LITE_JS,
      IDR_ESIM_MANAGER_MOJOM_HTML,
      IDR_ESIM_MANAGER_MOJOM_LITE_JS,
      IDR_MULTIDEVICE_DEVICE_SYNC_MOJOM_HTML,
      IDR_MULTIDEVICE_DEVICE_SYNC_MOJOM_LITE_JS,
      IDR_MULTIDEVICE_MULTIDEVICE_SETUP_MOJOM_HTML,
      IDR_MULTIDEVICE_MULTIDEVICE_SETUP_MOJOM_LITE_JS,
      IDR_MULTIDEVICE_MULTIDEVICE_TYPES_MOJOM_HTML,
      IDR_MULTIDEVICE_MULTIDEVICE_TYPES_MOJOM_LITE_JS,
      IDR_NETWORK_CONFIG_MOJOM_HTML,
      IDR_NETWORK_CONFIG_MOJOM_LITE_JS,
      IDR_NETWORK_CONFIG_TYPES_MOJOM_HTML,
      IDR_NETWORK_CONFIG_TYPES_MOJOM_LITE_JS,
      IDR_IP_ADDRESS_MOJOM_HTML,
      IDR_IP_ADDRESS_MOJOM_LITE_JS,
      IDR_NETWORK_HEALTH_MOJOM_HTML,
      IDR_NETWORK_HEALTH_MOJOM_LITE_JS,
      IDR_NETWORK_DIAGNOSTICS_MOJOM_HTML,
      IDR_NETWORK_DIAGNOSTICS_MOJOM_LITE_JS,
  };
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

void AddResource(const std::string& path,
                 int resource_id,
                 ResourcesMap* resources_map) {
  if (!resources_map->insert(std::make_pair(path, resource_id)).second)
    NOTREACHED() << "Redefinition of '" << path << "'";
}

// Adds all resources with IDs in |resource_ids| to |resources_map|.
void AddResources(const std::set<int>& resource_ids,
                  const webui::ResourcePath resources[],
                  size_t resources_size,
                  ResourcesMap* resources_map) {
  for (size_t i = 0; i < resources_size; ++i) {
    const auto& resource = resources[i];

    const auto it = resource_ids.find(resource.id);
    if (it == resource_ids.end())
      continue;

    AddResource(resource.path, resource.id, resources_map);
  }
}

// Adds |resources| to |resources_map| using the path given by resource_path in
// each GRD entry.
void AddGritResourcesToMap(base::span<const webui::ResourcePath> resources,
                           ResourcesMap* resources_map) {
  for (const webui::ResourcePath& entry : resources)
    AddResource(entry.path, entry.id, resources_map);
}

const ResourcesMap* CreateResourcesMap() {
  ResourcesMap* result = new ResourcesMap();
  AddGritResourcesToMap(base::make_span(kWebuiResources, kWebuiResourcesSize),
                        result);
  AddResources(GetContentResourceIds(), kContentResources,
               kContentResourcesSize, result);
  AddGritResourcesToMap(
      base::make_span(kMediaInternalsResources, kMediaInternalsResourcesSize),
      result);
  AddGritResourcesToMap(
      base::make_span(kWebuiGeneratedResources, kWebuiGeneratedResourcesSize),
      result);
  AddGritResourcesToMap(
      base::make_span(kMojoBindingsResources, kMojoBindingsResourcesSize),
      result);
  AddGritResourcesToMap(base::make_span(kSkiaResources, kSkiaResourcesSize),
                        result);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  AddResources(GetChromeosMojoResourceIds(), kChromeosResources,
               kChromeosResourcesSize, result);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
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
