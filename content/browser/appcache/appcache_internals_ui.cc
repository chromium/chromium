// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_internals_ui.h"

#include <stddef.h>
#include <vector>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_disk_cache_ops.h"
#include "content/browser/appcache/appcache_response_info.h"
#include "content/browser/storage_partition_impl.h"
#include "content/grit/dev_ui_content_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/view_cache_helper.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "ui/base/text/bytes_formatting.h"

namespace content {

namespace {

int64_t ToInt64(const std::string& str) {
  int64_t i = 0;
  base::StringToInt64(str.c_str(), &i);
  return i;
}

bool SortByResourceUrl(const blink::mojom::AppCacheResourceInfo& lhs,
                       const blink::mojom::AppCacheResourceInfo& rhs) {
  return lhs.url.spec() < rhs.url.spec();
}

base::Value GetDictionaryValueForResponseEnquiry(
    const AppCacheInternalsHandler::ProxyResponseEnquiry& response_enquiry) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("manifestURL", response_enquiry.manifest_url);
  dict.SetStringKey("groupId", base::NumberToString(response_enquiry.group_id));
  dict.SetStringKey("responseId",
                    base::NumberToString(response_enquiry.response_id));
  return dict;
}

base::Value GetDictionaryValueForAppCacheInfo(
    const blink::mojom::AppCacheInfo& appcache_info) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("manifestURL", appcache_info.manifest_url.spec());
  dict.SetDoubleKey("creationTime", appcache_info.creation_time.ToJsTime());
  dict.SetDoubleKey("lastUpdateTime",
                    appcache_info.last_update_time.ToJsTime());
  dict.SetDoubleKey("lastAccessTime",
                    appcache_info.last_access_time.ToJsTime());
  dict.SetDoubleKey("tokenExpires", appcache_info.token_expires.ToJsTime());
  dict.SetStringKey("responseSizes",
                    ui::FormatBytes(appcache_info.response_sizes));
  dict.SetStringKey("paddingSizes",
                    ui::FormatBytes(appcache_info.padding_sizes));
  dict.SetStringKey("totalSize", ui::FormatBytes(appcache_info.response_sizes +
                                                 appcache_info.padding_sizes));
  dict.SetStringKey("groupId", base::NumberToString(appcache_info.group_id));

  dict.SetStringKey(
      "manifestParserVersion",
      base::NumberToString(appcache_info.manifest_parser_version));
  dict.SetStringKey("manifestScope", appcache_info.manifest_scope);

  return dict;
}

base::Value GetListValueForAppCacheInfoVector(
    const std::vector<blink::mojom::AppCacheInfo> appcache_info_vector) {
  base::Value list(base::Value::Type::LIST);
  for (const blink::mojom::AppCacheInfo& info : appcache_info_vector)
    list.Append(GetDictionaryValueForAppCacheInfo(info));
  return list;
}

base::Value GetListValueFromAppCacheInfoCollection(
    AppCacheInfoCollection* appcache_collection) {
  base::Value list(base::Value::Type::LIST);
  for (const auto& key_value : appcache_collection->infos_by_origin) {
    base::Value dict(base::Value::Type::DICTIONARY);
    // Use GURL::spec() to keep consistency with previous version
    dict.SetStringKey("originURL", key_value.first.GetURL().spec());
    dict.SetKey("manifests",
                GetListValueForAppCacheInfoVector(key_value.second));
    list.Append(std::move(dict));
  }
  return list;
}

base::Value GetDictionaryValueForAppCacheResourceInfo(
    const blink::mojom::AppCacheResourceInfo& resource_info) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("url", resource_info.url.spec());
  dict.SetStringKey("responseSize",
                    ui::FormatBytes(resource_info.response_size));
  dict.SetStringKey("paddingSize", ui::FormatBytes(resource_info.padding_size));
  dict.SetStringKey("totalSize", ui::FormatBytes(resource_info.response_size +
                                                 resource_info.padding_size));
  dict.SetStringKey("responseId",
                    base::NumberToString(resource_info.response_id));
  dict.SetBoolKey("isExplicit", resource_info.is_explicit);
  dict.SetBoolKey("isManifest", resource_info.is_manifest);
  dict.SetBoolKey("isMaster", resource_info.is_master);
  dict.SetBoolKey("isFallback", resource_info.is_fallback);
  dict.SetBoolKey("isIntercept", resource_info.is_intercept);
  dict.SetBoolKey("isForeign", resource_info.is_foreign);

  return dict;
}

base::Value GetListValueForAppCacheResourceInfoVector(
    std::vector<blink::mojom::AppCacheResourceInfo>* resource_info_vector) {
  base::Value list(base::Value::Type::LIST);
  for (const blink::mojom::AppCacheResourceInfo& res_info :
       *resource_info_vector) {
    list.Append(GetDictionaryValueForAppCacheResourceInfo(res_info));
  }
  return list;
}

}  // namespace

AppCacheInternalsHandler::Proxy::Proxy(
    base::WeakPtr<AppCacheInternalsHandler> appcache_internals_handler,
    const base::FilePath& partition_path)
    : appcache_internals_handler_(appcache_internals_handler),
      partition_path_(partition_path) {}

void AppCacheInternalsHandler::Proxy::Initialize(
    const scoped_refptr<ChromeAppCacheService>& chrome_appcache_service) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&Proxy::Initialize, this, chrome_appcache_service));
    return;
  }
  appcache_service_ = chrome_appcache_service->AsWeakPtr();
  shutdown_called_ = false;
  preparing_response_ = false;
}

AppCacheInternalsHandler::Proxy::~Proxy() {
  DCHECK(shutdown_called_);
}

void AppCacheInternalsHandler::Proxy::Shutdown() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                        base::BindOnce(&Proxy::Shutdown, this));
    return;
  }
  shutdown_called_ = true;
  if (appcache_service_) {
    appcache_service_->storage()->CancelDelegateCallbacks(this);
    appcache_service_.reset();
    response_enquiries_.clear();
  }
}

void AppCacheInternalsHandler::Proxy::RequestAllAppCacheInfo() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&Proxy::RequestAllAppCacheInfo, this));
    return;
  }
  if (appcache_service_) {
    auto collection = base::MakeRefCounted<AppCacheInfoCollection>();
    AppCacheInfoCollection* collection_ptr = collection.get();
    appcache_service_->GetAllAppCacheInfo(
        collection_ptr, base::BindOnce(&Proxy::OnAllAppCacheInfoReady, this,
                                       std::move(collection)));
  }
}

void AppCacheInternalsHandler::Proxy::OnAllAppCacheInfoReady(
    scoped_refptr<AppCacheInfoCollection> collection,
    int net_result_code) {
  appcache_internals_handler_->OnAllAppCacheInfoReady(collection,
                                                      partition_path_);
}

void AppCacheInternalsHandler::Proxy::DeleteAppCache(
    const std::string& manifest_url,
    const std::string& callback_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&Proxy::DeleteAppCache, this, manifest_url,
                                  callback_id));
    return;
  }
  if (appcache_service_) {
    appcache_service_->DeleteAppCacheGroup(
        GURL(manifest_url),
        base::BindOnce(&Proxy::OnAppCacheInfoDeleted, this, callback_id));
  }
}

void AppCacheInternalsHandler::Proxy::OnAppCacheInfoDeleted(
    const std::string& callback_id,
    int net_result_code) {
  appcache_internals_handler_->OnAppCacheInfoDeleted(
      callback_id, net_result_code == net::OK);
}

void AppCacheInternalsHandler::Proxy::RequestAppCacheDetails(
    const std::string& manifest_url) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&Proxy::RequestAppCacheDetails, this, manifest_url));
    return;
  }

  if (appcache_service_)
    appcache_service_->storage()->LoadOrCreateGroup(GURL(manifest_url), this);
}

void AppCacheInternalsHandler::Proxy::OnGroupLoaded(
    AppCacheGroup* appcache_group,
    const GURL& manifest_gurl) {
  std::unique_ptr<std::vector<blink::mojom::AppCacheResourceInfo>>
      resource_info_vector;
  if (appcache_group && appcache_group->newest_complete_cache()) {
    resource_info_vector =
        std::make_unique<std::vector<blink::mojom::AppCacheResourceInfo>>();
    appcache_group->newest_complete_cache()->ToResourceInfoVector(
        resource_info_vector.get());
    std::sort(resource_info_vector->begin(), resource_info_vector->end(),
              SortByResourceUrl);
  }
  appcache_internals_handler_->OnAppCacheDetailsReady(
      partition_path_, manifest_gurl.spec(), std::move(resource_info_vector));
}

void AppCacheInternalsHandler::Proxy::RequestFileDetails(
    const ProxyResponseEnquiry& response_enquiry) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&Proxy::RequestFileDetails, this, response_enquiry));
    return;
  }
  DCHECK(!shutdown_called_);
  response_enquiries_.push_back(response_enquiry);
  HandleFileDetailsRequest();
}

void AppCacheInternalsHandler::Proxy::HandleFileDetailsRequest() {
  if (preparing_response_ || response_enquiries_.empty() || !appcache_service_)
    return;
  preparing_response_ = true;
  appcache_service_->storage()->LoadResponseInfo(
      GURL(response_enquiries_.front().manifest_url),
      response_enquiries_.front().response_id, this);
}

void AppCacheInternalsHandler::Proxy::OnResponseInfoLoaded(
    AppCacheResponseInfo* response,
    int64_t response_id) {
  if (shutdown_called_)
    return;
  if (!appcache_service_)
    return;
  ProxyResponseEnquiry response_enquiry = response_enquiries_.front();
  response_enquiries_.pop_front();
  if (response) {
    scoped_refptr<AppCacheResponseInfo> response_info = response;
    const int64_t kLimit = 100 * 1000;
    int64_t amount_to_read =
        std::min(kLimit, response_info->response_data_size());
    scoped_refptr<net::IOBuffer> response_data =
        base::MakeRefCounted<net::IOBuffer>(
            base::checked_cast<size_t>(amount_to_read));
    std::unique_ptr<AppCacheResponseReader> reader =
        appcache_service_->storage()->CreateResponseReader(
            GURL(response_enquiry.manifest_url), response_enquiry.response_id);
    AppCacheResponseReader* const reader_ptr = reader.get();

    reader_ptr->ReadData(response_data.get(), amount_to_read,
                         base::BindOnce(&Proxy::OnResponseDataReadComplete,
                                        this, response_enquiry, response_info,
                                        std::move(reader), response_data));
  } else {
    OnResponseDataReadComplete(response_enquiry, nullptr, nullptr, nullptr, -1);
  }
}

void AppCacheInternalsHandler::Proxy::OnResponseDataReadComplete(
    const ProxyResponseEnquiry& response_enquiry,
    scoped_refptr<AppCacheResponseInfo> response_info,
    std::unique_ptr<AppCacheResponseReader> reader,
    scoped_refptr<net::IOBuffer> response_data,
    int net_result_code) {
  if (shutdown_called_)
    return;
  if (!response_info || net_result_code < 0) {
    appcache_internals_handler_->OnFileDetailsFailed(response_enquiry,
                                                     net_result_code);
  } else {
    appcache_internals_handler_->OnFileDetailsReady(
        response_enquiry, response_info, response_data, net_result_code);
  }
  preparing_response_ = false;
  HandleFileDetailsRequest();
}

AppCacheInternalsUI::AppCacheInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<AppCacheInternalsHandler>());
  WebUIDataSource* source =
      WebUIDataSource::Create(kChromeUIAppCacheInternalsHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-eval';");
  source->DisableTrustedTypesCSP();

  source->UseStringsJs();
  source->AddResourcePath("appcache_internals.js", IDR_APPCACHE_INTERNALS_JS);
  source->AddResourcePath("appcache_internals.css", IDR_APPCACHE_INTERNALS_CSS);
  source->SetDefaultResource(IDR_APPCACHE_INTERNALS_HTML);

  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, source);
}

AppCacheInternalsUI::~AppCacheInternalsUI() = default;

AppCacheInternalsHandler::AppCacheInternalsHandler() = default;

AppCacheInternalsHandler::~AppCacheInternalsHandler() = default;

void AppCacheInternalsHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "getAllAppCache",
      base::BindRepeating(&AppCacheInternalsHandler::HandleGetAllAppCache,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "deleteAppCache",
      base::BindRepeating(&AppCacheInternalsHandler::HandleDeleteAppCache,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "getAppCacheDetails",
      base::BindRepeating(&AppCacheInternalsHandler::HandleGetAppCacheDetails,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "getFileDetails",
      base::BindRepeating(&AppCacheInternalsHandler::HandleGetFileDetails,
                          base::Unretained(this)));
}

void AppCacheInternalsHandler::OnJavascriptAllowed() {
  GetBrowserContext()->ForEachStoragePartition(base::BindRepeating(
      &AppCacheInternalsHandler::CreateProxyForPartition, AsWeakPtr()));
}

void AppCacheInternalsHandler::OnJavascriptDisallowed() {
  for (auto& proxy : appcache_proxies_)
    proxy->Shutdown();
  weak_ptr_factory_.InvalidateWeakPtrs();
  appcache_proxies_.clear();
}

void AppCacheInternalsHandler::CreateProxyForPartition(
    StoragePartition* storage_partition) {
  auto proxy = base::MakeRefCounted<Proxy>(weak_ptr_factory_.GetWeakPtr(),
                                           storage_partition->GetPath());
  auto* appcache_service = static_cast<StoragePartitionImpl*>(storage_partition)
                               ->GetAppCacheService();
  if (appcache_service)
    proxy->Initialize(appcache_service);
  appcache_proxies_.emplace_back(std::move(proxy));
}

void AppCacheInternalsHandler::HandleGetAllAppCache(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // This message indicates the WebUI page has loaded. Allow Javascript.
  AllowJavascript();
  for (scoped_refptr<Proxy>& proxy : appcache_proxies_)
    proxy->RequestAllAppCacheInfo();
}

void AppCacheInternalsHandler::HandleDeleteAppCache(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string manifest_url, partition_path;
  std::string callback_id;
  bool success = args->GetString(0, &callback_id) &&
                 args->GetString(1, &partition_path) &&
                 args->GetString(2, &manifest_url);
  CHECK(success);

  Proxy* proxy =
      GetProxyForPartitionPath(base::FilePath::FromUTF8Unsafe(partition_path));
  if (proxy)
    proxy->DeleteAppCache(manifest_url, callback_id);
}

void AppCacheInternalsHandler::HandleGetAppCacheDetails(
    const base::ListValue* args) {
  std::string manifest_url, partition_path;
  args->GetString(0, &partition_path);
  args->GetString(1, &manifest_url);
  Proxy* proxy =
      GetProxyForPartitionPath(base::FilePath::FromUTF8Unsafe(partition_path));
  if (proxy)
    proxy->RequestAppCacheDetails(manifest_url);
}

void AppCacheInternalsHandler::HandleGetFileDetails(
    const base::ListValue* args) {
  std::string manifest_url, partition_path, group_id_str, response_id_str;
  args->GetString(0, &partition_path);
  args->GetString(1, &manifest_url);
  args->GetString(2, &group_id_str);
  args->GetString(3, &response_id_str);
  Proxy* proxy =
      GetProxyForPartitionPath(base::FilePath::FromUTF8Unsafe(partition_path));
  if (proxy)
    proxy->RequestFileDetails(
        {manifest_url, ToInt64(group_id_str), ToInt64(response_id_str)});
}

void AppCacheInternalsHandler::OnAllAppCacheInfoReady(
    scoped_refptr<AppCacheInfoCollection> collection,
    const base::FilePath& partition_path) {
  std::string incognito_path_prefix;
  if (GetBrowserContext()->IsOffTheRecord())
    incognito_path_prefix = "Incognito ";
  FireWebUIListener(
      "all-appcache-info-ready", base::Value(partition_path.AsUTF8Unsafe()),
      base::Value(incognito_path_prefix + partition_path.AsUTF8Unsafe()),
      GetListValueFromAppCacheInfoCollection(collection.get()));
}

void AppCacheInternalsHandler::OnAppCacheInfoDeleted(
    const std::string& callback_id,
    bool deleted) {
  ResolveJavascriptCallback(base::Value(callback_id), base::Value(deleted));
}

void AppCacheInternalsHandler::OnAppCacheDetailsReady(
    const base::FilePath& partition_path,
    const std::string& manifest_url,
    std::unique_ptr<std::vector<blink::mojom::AppCacheResourceInfo>>
        resource_info_vector) {
  if (resource_info_vector) {
    FireWebUIListener(
        "appcache-details-ready", base::Value(manifest_url),
        base::Value(partition_path.AsUTF8Unsafe()),
        GetListValueForAppCacheResourceInfoVector(resource_info_vector.get()));
  } else {
    FireWebUIListener("appcache-details-ready", base::Value(manifest_url),
                      base::Value(partition_path.AsUTF8Unsafe()));
  }
}

void AppCacheInternalsHandler::OnFileDetailsReady(
    const ProxyResponseEnquiry& response_enquiry,
    scoped_refptr<AppCacheResponseInfo> response_info,
    scoped_refptr<net::IOBuffer> response_data,
    int data_length) {
  std::string headers;
  headers.append("<hr><pre>");
  headers.append(net::EscapeForHTML(
      response_info->http_response_info().headers->GetStatusLine()));
  headers.push_back('\n');

  size_t iter = 0;
  std::string name, value;
  while (response_info->http_response_info().headers->EnumerateHeaderLines(
      &iter, &name, &value)) {
    headers.append(net::EscapeForHTML(name));
    headers.append(": ");
    headers.append(net::EscapeForHTML(value));
    headers.push_back('\n');
  }
  headers.append("</pre>");

  std::string hex_dump = base::StringPrintf(
      "<hr><pre> Showing %d of %d bytes\n\n", static_cast<int>(data_length),
      static_cast<int>(response_info->response_data_size()));
  net::ViewCacheHelper::HexDump(response_data->data(), data_length, &hex_dump);
  if (data_length < response_info->response_data_size())
    hex_dump.append("\nNote: data is truncated...");
  hex_dump.append("</pre>");
  FireWebUIListener("file-details-ready",
                    GetDictionaryValueForResponseEnquiry(response_enquiry),
                    base::Value(headers), base::Value(hex_dump));
}

void AppCacheInternalsHandler::OnFileDetailsFailed(
    const ProxyResponseEnquiry& response_enquiry,
    int net_result_code) {
  FireWebUIListener("file-details-failed",
                    GetDictionaryValueForResponseEnquiry(response_enquiry),
                    base::Value(net_result_code));
}

BrowserContext* AppCacheInternalsHandler::GetBrowserContext() {
  return web_ui()->GetWebContents()->GetBrowserContext();
}

AppCacheInternalsHandler::Proxy*
AppCacheInternalsHandler::GetProxyForPartitionPath(
    const base::FilePath& partition_path) {
  for (const scoped_refptr<Proxy>& proxy : appcache_proxies_) {
    if (proxy->partition_path_ == partition_path)
      return proxy.get();
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace content
