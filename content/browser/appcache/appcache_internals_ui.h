// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_INTERNALS_UI_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_INTERNALS_UI_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_storage.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/io_buffer.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom-forward.h"

namespace base {
class ListValue;
class FilePath;
}

namespace content {

// The implementation for the chrome://appcache-internals page.
// This implementation is based on the WebUI API and consists of a controller on
// The UI thread which communicates (through a Proxy) with the AppCacheService
// and AppCache storage which live on the IO thread.
class AppCacheInternalsUI : public WebUIController {
 public:
  explicit AppCacheInternalsUI(WebUI* web_ui);

  AppCacheInternalsUI(const AppCacheInternalsUI&) = delete;
  AppCacheInternalsUI& operator=(const AppCacheInternalsUI&) = delete;

  ~AppCacheInternalsUI() override;
};

class AppCacheInternalsHandler : public WebUIMessageHandler {
 public:
  struct ProxyResponseEnquiry {
    std::string manifest_url;
    int64_t group_id;
    int64_t response_id;
  };

  AppCacheInternalsHandler();

  AppCacheInternalsHandler(const AppCacheInternalsHandler&) = delete;
  AppCacheInternalsHandler& operator=(const AppCacheInternalsHandler&) = delete;

  ~AppCacheInternalsHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;
  void OnJavascriptAllowed() override;

  base::WeakPtr<AppCacheInternalsHandler> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  class Proxy : public AppCacheStorage::Delegate,
                public base::RefCountedThreadSafe<Proxy> {
   public:
    friend class AppCacheInternalsHandler;

    Proxy(base::WeakPtr<AppCacheInternalsHandler> appcache_internals_handler,
          const base::FilePath& storage_partition);

   private:
    friend class base::RefCountedThreadSafe<Proxy>;

    ~Proxy() override;

    void RequestAllAppCacheInfo();
    void DeleteAppCache(const std::string& manifest_url,
                        const std::string& callback_id);
    void RequestAppCacheDetails(const std::string& manifest_url);
    void RequestFileDetails(const ProxyResponseEnquiry& response_enquiry);
    void HandleFileDetailsRequest();
    void OnAllAppCacheInfoReady(
        scoped_refptr<AppCacheInfoCollection> collection,
        int net_result_code);
    void OnAppCacheInfoDeleted(const std::string& callback_id,
                               int net_result_code);
    void OnGroupLoaded(AppCacheGroup* appcache_group,
                       const GURL& manifest_gurl) override;
    void OnResponseInfoLoaded(AppCacheResponseInfo* response_info,
                              int64_t response_id) override;
    void OnResponseDataReadComplete(
        const ProxyResponseEnquiry& response_enquiry,
        scoped_refptr<AppCacheResponseInfo> response_info,
        std::unique_ptr<AppCacheResponseReader> reader,
        scoped_refptr<net::IOBuffer> response_data,
        int net_result_code);
    void Initialize(
        const scoped_refptr<ChromeAppCacheService>& chrome_appcache_service);
    void Shutdown();

    base::WeakPtr<AppCacheInternalsHandler> appcache_internals_handler_;
    base::WeakPtr<AppCacheServiceImpl> appcache_service_;
    base::FilePath partition_path_;
    scoped_refptr<AppCacheStorageReference> disabled_appcache_storage_ref_;
    std::list<ProxyResponseEnquiry> response_enquiries_;
    bool preparing_response_;
    bool shutdown_called_;
  };

  void CreateProxyForPartition(StoragePartition* storage_partition);
  // Commands from Javascript side.
  void HandleGetAllAppCache(const base::ListValue* args);
  void HandleDeleteAppCache(const base::ListValue* args);
  void HandleGetAppCacheDetails(const base::ListValue* args);
  void HandleGetFileDetails(const base::ListValue* args);

  // Results from commands to be sent to Javascript.
  void OnAllAppCacheInfoReady(scoped_refptr<AppCacheInfoCollection> collection,
                              const base::FilePath& partition_path);
  void OnAppCacheInfoDeleted(const std::string& callback_id, bool deleted);
  void OnAppCacheDetailsReady(
      const base::FilePath& partition_path,
      const std::string& manifest_url,
      std::unique_ptr<std::vector<blink::mojom::AppCacheResourceInfo>>
          resource_info_vector);
  void OnFileDetailsReady(const ProxyResponseEnquiry& response_enquiry,
                          scoped_refptr<AppCacheResponseInfo> response_info,
                          scoped_refptr<net::IOBuffer> response_data,
                          int data_length);
  void OnFileDetailsFailed(const ProxyResponseEnquiry& response_enquiry,
                           int data_length);

  BrowserContext* GetBrowserContext();

  Proxy* GetProxyForPartitionPath(const base::FilePath& path);
  std::list<scoped_refptr<Proxy>> appcache_proxies_;
  base::WeakPtrFactory<AppCacheInternalsHandler> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_INTERNALS_UI_H_
