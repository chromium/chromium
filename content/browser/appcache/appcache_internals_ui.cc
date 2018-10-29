// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_internals_ui.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/storage_partition_impl.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/view_cache_helper.h"

namespace content {

namespace {
const char kRequestGetAllAppCacheInfo[] = "getAllAppCache";
const char kRequestDeleteAppCache[] = "deleteAppCache";
const char kRequestGetAppCacheDetails[] = "getAppCacheDetails";
const char kRequestGetFileDetails[] = "getFileDetails";

const char kFunctionOnAllAppCacheInfoReady[] =
    "appcache.onAllAppCacheInfoReady";
const char kFunctionOnAppCacheInfoDeleted[] = "appcache.onAppCacheInfoDeleted";
const char kFunctionOnAppCacheDetailsReady[] =
    "appcache.onAppCacheDetailsReady";
const char kFunctionOnFileDetailsReady[] = "appcache.onFileDetailsReady";
const char kFunctionOnFileDetailsFailed[] = "appcache.onFileDetailsFailed";

int64_t ToInt64(const std::string& str) {
  int64_t i = 0;
  base::StringToInt64(str.c_str(), &i);
  return i;
}

bool SortByResourceUrl(const AppCacheResourceInfo& lhs,
                       const AppCacheResourceInfo& rhs) {
  return lhs.url.spec() < rhs.url.spec();
}

std::unique_ptr<base::DictionaryValue> GetDictionaryValueForResponseEnquiry(
    const content::AppCacheInternalsUI::Proxy::ResponseEnquiry&
        response_enquiry) {
  std::unique_ptr<base::DictionaryValue> dict_value(
      new base::DictionaryValue());
  dict_value->SetString("manifestURL", response_enquiry.manifest_url);
  dict_value->SetString("groupId",
                        base::Int64ToString(response_enquiry.group_id));
  dict_value->SetString("responseId",
                        base::Int64ToString(response_enquiry.response_id));
  return dict_value;
}

std::unique_ptr<base::DictionaryValue> GetDictionaryValueForAppCacheInfo(
    const content::AppCacheInfo& appcache_info) {
  std::unique_ptr<base::DictionaryValue> dict_value(
      new base::DictionaryValue());
  dict_value->SetString("manifestURL", appcache_info.manifest_url.spec());
  dict_value->SetDouble("creationTime", appcache_info.creation_time.ToJsTime());
  dict_value->SetDouble("lastUpdateTime",
                        appcache_info.last_update_time.ToJsTime());
  dict_value->SetDouble("lastAccessTime",
                        appcache_info.last_access_time.ToJsTime());
  dict_value->SetString(
      "size",
      base::UTF16ToUTF8(base::FormatBytesUnlocalized(appcache_info.size)));
  dict_value->SetString("groupId", base::Int64ToString(appcache_info.group_id));

  return dict_value;
}

std::unique_ptr<base::ListValue> GetListValueForAppCacheInfoVector(
    const AppCacheInfoVector& appcache_info_vector) {
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  for (const AppCacheInfo& info : appcache_info_vector)
    list->Append(GetDictionaryValueForAppCacheInfo(info));
  return list;
}

std::unique_ptr<base::ListValue> GetListValueFromAppCacheInfoCollection(
    AppCacheInfoCollection* appcache_collection) {
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  for (const auto& key_value : appcache_collection->infos_by_origin) {
    base::DictionaryValue* dict = new base::DictionaryValue;
    // Use GURL::spec() to keep consistency with previous version
    dict->SetString("originURL", key_value.first.GetURL().spec());
    dict->Set("manifests", GetListValueForAppCacheInfoVector(key_value.second));
    list->Append(std::unique_ptr<base::Value>(dict));
  }
  return list;
}

std::unique_ptr<base::DictionaryValue>
GetDictionaryValueForAppCacheResourceInfo(
    const AppCacheResourceInfo& resource_info) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("url", resource_info.url.spec());
  dict->SetString(
      "size",
      base::UTF16ToUTF8(base::FormatBytesUnlocalized(resource_info.size)));
  dict->SetString("responseId", base::Int64ToString(resource_info.response_id));
  dict->SetBoolean("isExplicit", resource_info.is_explicit);
  dict->SetBoolean("isManifest", resource_info.is_manifest);
  dict->SetBoolean("isMaster", resource_info.is_master);
  dict->SetBoolean("isFallback", resource_info.is_fallback);
  dict->SetBoolean("isIntercept", resource_info.is_intercept);
  dict->SetBoolean("isForeign", resource_info.is_foreign);

  return dict;
}

std::unique_ptr<base::ListValue> GetListValueForAppCacheResourceInfoVector(
    std::vector<AppCacheResourceInfo>* resource_info_vector) {
  std::unique_ptr<base::ListValue> list(new base::ListValue);
  for (const AppCacheResourceInfo& res_info : *resource_info_vector)
    list->Append(GetDictionaryValueForAppCacheResourceInfo(res_info));
  return list;
}

}  // namespace

AppCacheInternalsUI::Proxy::Proxy(
    base::WeakPtr<AppCacheInternalsUI> appcache_internals_ui,
    const base::FilePath& partition_path)
    : appcache_internals_ui_(appcache_internals_ui),
      partition_path_(partition_path) {}

void AppCacheInternalsUI::Proxy::Initialize(
    const scoped_refptr<ChromeAppCacheService>& chrome_appcache_service) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&Proxy::Initialize, this, chrome_appcache_service));
    return;
  }
  appcache_service_ = chrome_appcache_service->AsWeakPtr();
  shutdown_called_ = false;
  preparing_response_ = false;
}

AppCacheInternalsUI::Proxy::~Proxy() {
  DCHECK(shutdown_called_);
}

void AppCacheInternalsUI::Proxy::Shutdown() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
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

void AppCacheInternalsUI::Proxy::RequestAllAppCacheInfo() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&Proxy::RequestAllAppCacheInfo, this));
    return;
  }
  if (appcache_service_) {
    scoped_refptr<AppCacheInfoCollection> collection(
        new AppCacheInfoCollection());
    appcache_service_->GetAllAppCacheInfo(
        collection.get(),
        base::BindOnce(&Proxy::OnAllAppCacheInfoReady, this, collection));
  }
}

void AppCacheInternalsUI::Proxy::OnAllAppCacheInfoReady(
    scoped_refptr<AppCacheInfoCollection> collection,
    int net_result_code) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&AppCacheInternalsUI::OnAllAppCacheInfoReady,
                     appcache_internals_ui_, collection, partition_path_));
}

void AppCacheInternalsUI::Proxy::DeleteAppCache(
    const std::string& manifest_url) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&Proxy::DeleteAppCache, this, manifest_url));
    return;
  }
  if (appcache_service_) {
    appcache_service_->DeleteAppCacheGroup(
        GURL(manifest_url),
        base::BindOnce(&Proxy::OnAppCacheInfoDeleted, this, manifest_url));
  }
}

void AppCacheInternalsUI::Proxy::OnAppCacheInfoDeleted(
    const std::string& manifest_url,
    int net_result_code) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&AppCacheInternalsUI::OnAppCacheInfoDeleted,
                     appcache_internals_ui_, partition_path_, manifest_url,
                     net_result_code == net::OK));
}

void AppCacheInternalsUI::Proxy::RequestAppCacheDetails(
    const std::string& manifest_url) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&Proxy::RequestAppCacheDetails, this, manifest_url));
    return;
  }

  if (appcache_service_)
    appcache_service_->storage()->LoadOrCreateGroup(GURL(manifest_url), this);
}

void AppCacheInternalsUI::Proxy::OnGroupLoaded(AppCacheGroup* appcache_group,
                                               const GURL& manifest_gurl) {
  std::unique_ptr<std::vector<AppCacheResourceInfo>> resource_info_vector;
  if (appcache_group && appcache_group->newest_complete_cache()) {
    resource_info_vector.reset(new std::vector<AppCacheResourceInfo>);
    appcache_group->newest_complete_cache()->ToResourceInfoVector(
        resource_info_vector.get());
    std::sort(resource_info_vector->begin(), resource_info_vector->end(),
              SortByResourceUrl);
  }
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&AppCacheInternalsUI::OnAppCacheDetailsReady,
                     appcache_internals_ui_, partition_path_,
                     manifest_gurl.spec(), std::move(resource_info_vector)));
}

void AppCacheInternalsUI::Proxy::RequestFileDetails(
    const ResponseEnquiry& response_enquiry) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&Proxy::RequestFileDetails, this, response_enquiry));
    return;
  }
  DCHECK(!shutdown_called_);
  response_enquiries_.push_back(response_enquiry);
  HandleFileDetailsRequest();
}

void AppCacheInternalsUI::Proxy::HandleFileDetailsRequest() {
  if (preparing_response_ || response_enquiries_.empty() || !appcache_service_)
    return;
  preparing_response_ = true;
  appcache_service_->storage()->LoadResponseInfo(
      GURL(response_enquiries_.front().manifest_url),
      response_enquiries_.front().response_id, this);
}

void AppCacheInternalsUI::Proxy::OnResponseInfoLoaded(
    AppCacheResponseInfo* response,
    int64_t response_id) {
  if (shutdown_called_)
    return;
  if (!appcache_service_)
    return;
  ResponseEnquiry response_enquiry = response_enquiries_.front();
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

    reader->ReadData(response_data.get(), amount_to_read,
                     base::BindOnce(&Proxy::OnResponseDataReadComplete, this,
                                    response_enquiry, response_info,
                                    std::move(reader), response_data));
  } else {
    OnResponseDataReadComplete(response_enquiry, nullptr, nullptr, nullptr, -1);
  }
}

void AppCacheInternalsUI::Proxy::OnResponseDataReadComplete(
    const ResponseEnquiry& response_enquiry,
    scoped_refptr<AppCacheResponseInfo> response_info,
    std::unique_ptr<AppCacheResponseReader> reader,
    scoped_refptr<net::IOBuffer> response_data,
    int net_result_code) {
  if (shutdown_called_)
    return;
  if (!response_info || net_result_code < 0) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&AppCacheInternalsUI::OnFileDetailsFailed,
                       appcache_internals_ui_, response_enquiry,
                       net_result_code));
  } else {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&AppCacheInternalsUI::OnFileDetailsReady,
                       appcache_internals_ui_, response_enquiry, response_info,
                       response_data, net_result_code));
  }
  preparing_response_ = false;
  HandleFileDetailsRequest();
}

AppCacheInternalsUI::AppCacheInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui), weak_ptr_factory_(this) {
  web_ui->RegisterMessageCallback(
      kRequestGetAllAppCacheInfo,
      base::BindRepeating(&AppCacheInternalsUI::GetAllAppCache, AsWeakPtr()));

  web_ui->RegisterMessageCallback(
      kRequestDeleteAppCache,
      base::BindRepeating(&AppCacheInternalsUI::DeleteAppCache, AsWeakPtr()));

  web_ui->RegisterMessageCallback(
      kRequestGetAppCacheDetails,
      base::BindRepeating(&AppCacheInternalsUI::GetAppCacheDetails,
                          AsWeakPtr()));

  web_ui->RegisterMessageCallback(
      kRequestGetFileDetails,
      base::BindRepeating(&AppCacheInternalsUI::GetFileDetails, AsWeakPtr()));

  WebUIDataSource* source =
      WebUIDataSource::Create(kChromeUIAppCacheInternalsHost);
  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources 'self' 'unsafe-eval';");

  source->SetJsonPath("strings.js");
  source->AddResourcePath("appcache_internals.js", IDR_APPCACHE_INTERNALS_JS);
  source->AddResourcePath("appcache_internals.css", IDR_APPCACHE_INTERNALS_CSS);
  source->SetDefaultResource(IDR_APPCACHE_INTERNALS_HTML);
  source->UseGzip();

  WebUIDataSource::Add(browser_context(), source);

  BrowserContext::StoragePartitionCallback callback = base::BindRepeating(
      &AppCacheInternalsUI::CreateProxyForPartition, AsWeakPtr());
  BrowserContext::ForEachStoragePartition(browser_context(), callback);
}

AppCacheInternalsUI::~AppCacheInternalsUI() {
  for (auto& proxy : appcache_proxies_)
    proxy->Shutdown();
}

void AppCacheInternalsUI::CreateProxyForPartition(
    StoragePartition* storage_partition) {
  scoped_refptr<Proxy> proxy =
      new Proxy(weak_ptr_factory_.GetWeakPtr(), storage_partition->GetPath());
  proxy->Initialize(static_cast<StoragePartitionImpl*>(storage_partition)
                        ->GetAppCacheService());
  appcache_proxies_.push_back(proxy);
}

void AppCacheInternalsUI::GetAllAppCache(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (scoped_refptr<Proxy>& proxy : appcache_proxies_)
    proxy->RequestAllAppCacheInfo();
}

void AppCacheInternalsUI::DeleteAppCache(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string manifest_url, partition_path;
  args->GetString(0, &partition_path);
  args->GetString(1, &manifest_url);
  Proxy* proxy =
      GetProxyForPartitionPath(base::FilePath::FromUTF8Unsafe(partition_path));
  if (proxy)
    proxy->DeleteAppCache(manifest_url);
}

void AppCacheInternalsUI::GetAppCacheDetails(const base::ListValue* args) {
  std::string manifest_url, partition_path;
  args->GetString(0, &partition_path);
  args->GetString(1, &manifest_url);
  Proxy* proxy =
      GetProxyForPartitionPath(base::FilePath::FromUTF8Unsafe(partition_path));
  if (proxy)
    proxy->RequestAppCacheDetails(manifest_url);
}

void AppCacheInternalsUI::GetFileDetails(const base::ListValue* args) {
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

void AppCacheInternalsUI::OnAllAppCacheInfoReady(
    scoped_refptr<AppCacheInfoCollection> collection,
    const base::FilePath& partition_path) {
  std::string incognito_path_prefix;
  if (browser_context()->IsOffTheRecord())
    incognito_path_prefix = "Incognito ";
  web_ui()->CallJavascriptFunctionUnsafe(
      kFunctionOnAllAppCacheInfoReady,
      base::Value(incognito_path_prefix + partition_path.AsUTF8Unsafe()),
      *GetListValueFromAppCacheInfoCollection(collection.get()));
}

void AppCacheInternalsUI::OnAppCacheInfoDeleted(
    const base::FilePath& partition_path,
    const std::string& manifest_url,
    bool deleted) {
  web_ui()->CallJavascriptFunctionUnsafe(
      kFunctionOnAppCacheInfoDeleted,
      base::Value(partition_path.AsUTF8Unsafe()), base::Value(manifest_url),
      base::Value(deleted));
}

void AppCacheInternalsUI::OnAppCacheDetailsReady(
    const base::FilePath& partition_path,
    const std::string& manifest_url,
    std::unique_ptr<std::vector<AppCacheResourceInfo>> resource_info_vector) {
  if (resource_info_vector) {
    web_ui()->CallJavascriptFunctionUnsafe(
        kFunctionOnAppCacheDetailsReady, base::Value(manifest_url),
        base::Value(partition_path.AsUTF8Unsafe()),
        *GetListValueForAppCacheResourceInfoVector(resource_info_vector.get()));
  } else {
    web_ui()->CallJavascriptFunctionUnsafe(
        kFunctionOnAppCacheDetailsReady, base::Value(manifest_url),
        base::Value(partition_path.AsUTF8Unsafe()));
  }
}

void AppCacheInternalsUI::OnFileDetailsReady(
    const Proxy::ResponseEnquiry& response_enquiry,
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
  web_ui()->CallJavascriptFunctionUnsafe(
      kFunctionOnFileDetailsReady,
      *GetDictionaryValueForResponseEnquiry(response_enquiry),
      base::Value(headers), base::Value(hex_dump));
}

void AppCacheInternalsUI::OnFileDetailsFailed(
    const Proxy::ResponseEnquiry& response_enquiry,
    int net_result_code) {
  web_ui()->CallJavascriptFunctionUnsafe(
      kFunctionOnFileDetailsFailed,
      *GetDictionaryValueForResponseEnquiry(response_enquiry),
      base::Value(net_result_code));
}

AppCacheInternalsUI::Proxy* AppCacheInternalsUI::GetProxyForPartitionPath(
    const base::FilePath& partition_path) {
  for (const scoped_refptr<Proxy>& proxy : appcache_proxies_) {
    if (proxy->partition_path_ == partition_path)
      return proxy.get();
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace content
