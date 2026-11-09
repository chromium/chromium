// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/shared_resources_data_source.h"

#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/task/thread_pool.h"
#include "content/browser/webui/web_ui_data_source_impl.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/resources/grit/webui_resources.h"
#include "ui/webui/resources/grit/webui_resources_map.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/webui/grit/ash_webui_common_resources_map.h"
#include "content/grit/content_resources.h"
#include "content/grit/content_resources_map.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"
#include "mojo/public/js/grit/mojo_bindings_resources_map.h"
#endif

namespace content {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
const std::set<int> GetContentResourceIds() {
  return std::set<int>{
      IDR_UNGUESSABLE_TOKEN_MOJO_JS,
      IDR_URL_MOJO_JS,
  };
}

// Adds all resources with IDs in |resource_ids| to |resources_map|.
void AddResources(const std::set<int>& resource_ids,
                  base::span<const webui::ResourcePath> resources,
                  WebUIDataSource* source) {
  for (const auto& resource : resources) {
    if (resource_ids.contains(resource.id)) {
      source->AddResourcePath(resource.path, resource.id);
    }
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

std::string_view GetCleanPath(std::string_view path) {
  return path.substr(0, path.find_first_of("?#"));
}

bool ShouldHandleUntrustedCssJsRequest(const WebUIDataSourceImpl* source,
                                       const std::string& path) {
  std::string_view clean_path = GetCleanPath(path);
  return clean_path.ends_with(".css.js") &&
         source->path_to_idr_map().contains(clean_path);
}

void ReadAndRewriteCssJsOnWorkerThread(
    int resource_id,
    WebUIDataSource::GotDataCallback callback) {
  ContentClient* content_client = GetContentClient();
  CHECK(content_client);
  scoped_refptr<base::RefCountedMemory> bytes =
      content_client->GetDataResourceBytes(resource_id);
  if (!bytes) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::string_view input = base::as_string_view(*bytes);
  constexpr std::string_view kChromeResourcesUrl = "url(chrome://resources/";
  if (!input.contains(kChromeResourcesUrl)) {
    std::move(callback).Run(std::move(bytes));
    return;
  }

  std::string content(input);
  base::ReplaceSubstringsAfterOffset(&content, 0, kChromeResourcesUrl,
                                     "url(//resources/");
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(content)));
}

void HandleUntrustedCssJsRequest(const WebUIDataSourceImpl* source,
                                 const std::string& path,
                                 WebUIDataSource::GotDataCallback callback) {
  const auto& map = source->path_to_idr_map();
  auto it = map.find(GetCleanPath(path));
  CHECK(it != map.end());
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()})
      ->PostTask(FROM_HERE, base::BindOnce(&ReadAndRewriteCssJsOnWorkerThread,
                                           it->second, std::move(callback)));
}

}  // namespace

void PopulateSharedResourcesDataSource(WebUIDataSourceImpl* source) {
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src blob: 'self';");

  // Note: Don't put generated Mojo bindings here. Mojo bindings should be
  // included in the either the ts_library() target for the UI using them (if
  // they are only used by one UI) or in //ui/webui/resources/mojo:build_ts
  // (if used by multiple UIs).
  // The shared resources are static assets bundled with Chrome, meant to be
  // readable by every WebUI origin (both chrome:// and chrome-untrusted://).
  source->SetAllowAllOrigins(true);

  source->AddResourcePaths(kWebuiResources);
#if BUILDFLAG(IS_CHROMEOS)
  source->AddResourcePaths(kAshWebuiCommonResources);
  // Deprecated -lite style mojo bindings.
  source->AddResourcePaths(kMojoBindingsResources);
  AddResources(GetContentResourceIds(), kContentResources, source);
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (source->GetOrigin().scheme() == kChromeUIUntrustedScheme) {
    // base::Unretained(source) is safe because `source` owns the request filter
    // callbacks and only invokes them from `source->StartDataRequest()`.
    source->SetRequestFilter(
        base::BindRepeating(&ShouldHandleUntrustedCssJsRequest,
                            base::Unretained(source)),
        base::BindRepeating(&HandleUntrustedCssJsRequest,
                            base::Unretained(source)));
  }
}

}  // namespace content
