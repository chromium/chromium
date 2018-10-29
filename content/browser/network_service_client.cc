// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network_service_client.h"

#include "base/optional.h"
#include "base/task/post_task.h"
#include "content/browser/browsing_data/clear_site_data_handler.h"
#include "content/browser/devtools/devtools_url_loader_interceptor.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/browser/ssl_private_key_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/ssl/client_cert_store.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {
namespace {

class SSLErrorDelegate : public SSLErrorHandler::Delegate {
 public:
  explicit SSLErrorDelegate(
      network::mojom::NetworkServiceClient::OnSSLCertificateErrorCallback
          response)
      : response_(std::move(response)), weak_factory_(this) {}
  ~SSLErrorDelegate() override {}
  void CancelSSLRequest(int error, const net::SSLInfo* ssl_info) override {
    std::move(response_).Run(error);
    delete this;
  }
  void ContinueSSLRequest() override {
    std::move(response_).Run(net::OK);
    delete this;
  }
  base::WeakPtr<SSLErrorDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  network::mojom::NetworkServiceClient::OnSSLCertificateErrorCallback response_;
  base::WeakPtrFactory<SSLErrorDelegate> weak_factory_;
};

// This class is created on UI thread, and deleted by
// BrowserThread::DeleteSoon() after the |callback_| runs. The |callback_|
// needs to run on UI thread since it is called through the
// NetworkServiceClient interface.
//
// The |ssl_client_auth_handler_| needs to be created on IO thread, and deleted
// on the same thread by posting a BrowserThread::DeleteSoon() task to IO
// thread.
//
// ContinueWithCertificate() and CancelCertificateSelection() run on IO thread.
class SSLClientAuthDelegate : public SSLClientAuthHandler::Delegate {
 public:
  SSLClientAuthDelegate(
      network::mojom::NetworkServiceClient::OnCertificateRequestedCallback
          callback,
      ResourceRequestInfo::WebContentsGetter web_contents_getter,
      scoped_refptr<net::SSLCertRequestInfo> cert_info)
      : callback_(std::move(callback)), cert_info_(cert_info) {
    content::WebContents* web_contents = web_contents_getter.Run();
    content::BrowserContext* browser_context =
        web_contents->GetBrowserContext();
    content::ResourceContext* resource_context =
        browser_context->GetResourceContext();
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&SSLClientAuthDelegate::CreateSSLClientAuthHandler,
                       base::Unretained(this), resource_context,
                       web_contents_getter));
  }
  ~SSLClientAuthDelegate() override {}

  // SSLClientAuthHandler::Delegate:
  void ContinueWithCertificate(
      scoped_refptr<net::X509Certificate> cert,
      scoped_refptr<net::SSLPrivateKey> private_key) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    DCHECK((cert && private_key) || (!cert && !private_key));

    std::vector<uint16_t> algorithm_preferences;
    network::mojom::SSLPrivateKeyPtr ssl_private_key;
    auto ssl_private_key_request = mojo::MakeRequest(&ssl_private_key);

    if (private_key) {
      algorithm_preferences = private_key->GetAlgorithmPreferences();
      mojo::MakeStrongBinding(
          std::make_unique<SSLPrivateKeyImpl>(std::move(private_key)),
          std::move(ssl_private_key_request));
    }

    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&SSLClientAuthDelegate::RunCallback,
                       base::Unretained(this), cert, algorithm_preferences,
                       std::move(ssl_private_key),
                       false /* cancel_certificate_selection */));
  }

  // SSLClientAuthHandler::Delegate:
  void CancelCertificateSelection() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    network::mojom::SSLPrivateKeyPtr ssl_private_key;
    mojo::MakeRequest(&ssl_private_key);
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&SSLClientAuthDelegate::RunCallback,
                       base::Unretained(this), nullptr, std::vector<uint16_t>(),
                       std::move(ssl_private_key),
                       true /* cancel_certificate_selection */));
  }

  void RunCallback(scoped_refptr<net::X509Certificate> cert,
                   std::vector<uint16_t> algorithm_preferences,
                   network::mojom::SSLPrivateKeyPtr ssl_private_key,
                   bool cancel_certificate_selection) {
    std::move(callback_).Run(cert, algorithm_preferences,
                             std::move(ssl_private_key),
                             cancel_certificate_selection);
    BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, this);
  }

 private:
  void CreateSSLClientAuthHandler(
      content::ResourceContext* resource_context,
      ResourceRequestInfo::WebContentsGetter web_contents_getter) {
    std::unique_ptr<net::ClientCertStore> client_cert_store =
        GetContentClient()->browser()->CreateClientCertStore(resource_context);
    ssl_client_auth_handler_.reset(new SSLClientAuthHandler(
        std::move(client_cert_store), std::move(web_contents_getter),
        cert_info_.get(), this));
    ssl_client_auth_handler_->SelectCertificate();
  }

  network::mojom::NetworkServiceClient::OnCertificateRequestedCallback
      callback_;
  scoped_refptr<net::SSLCertRequestInfo> cert_info_;
  std::unique_ptr<SSLClientAuthHandler> ssl_client_auth_handler_;
};

// This class is created on UI thread, and deleted by
// BrowserThread::DeleteSoon() after the |callback_| runs. The |callback_|
// needs to run on UI thread since it is called through the
// NetworkServiceClient interface.
//
// The |login_delegate_| needs to be created on IO thread, and deleted
// on the same thread by posting a BrowserThread::DeleteSoon() task to IO
// thread.
class LoginHandlerDelegate {
 public:
  LoginHandlerDelegate(
      network::mojom::AuthChallengeResponderPtr auth_challenge_responder,
      ResourceRequestInfo::WebContentsGetter web_contents_getter,
      scoped_refptr<net::AuthChallengeInfo> auth_info,
      bool is_request_for_main_frame,
      uint32_t process_id,
      uint32_t routing_id,
      uint32_t request_id,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt)
      : auth_challenge_responder_(std::move(auth_challenge_responder)),
        auth_info_(auth_info),
        request_id_(process_id, request_id),
        is_request_for_main_frame_(is_request_for_main_frame),
        url_(url),
        response_headers_(std::move(response_headers)),
        first_auth_attempt_(first_auth_attempt),
        web_contents_getter_(web_contents_getter) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auth_challenge_responder_.set_connection_error_handler(base::BindOnce(
        &LoginHandlerDelegate::OnRequestCancelled, base::Unretained(this)));

    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&LoginHandlerDelegate::DispatchInterceptorHookAndStart,
                       base::Unretained(this), process_id, routing_id,
                       request_id));
  }

  void OnRequestCancelled() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!login_delegate_)
      return;

    // LoginDelegate::OnRequestCancelled can only be called from the IO thread.
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&LoginHandlerDelegate::OnRequestCancelledOnIOThread,
                       base::Unretained(this)));
  }

  void OnRequestCancelledOnIOThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    login_delegate_->OnRequestCancelled();
  }

 private:
  void DispatchInterceptorHookAndStart(uint32_t process_id,
                                       uint32_t routing_id,
                                       uint32_t request_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DevToolsURLLoaderInterceptor::HandleAuthRequest(
        process_id, routing_id, request_id, auth_info_,
        base::BindOnce(&LoginHandlerDelegate::ContinueAfterInterceptor,
                       base::Unretained(this)));
  }

  void ContinueAfterInterceptor(
      bool use_fallback,
      const base::Optional<net::AuthCredentials>& auth_credentials) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(!(use_fallback && auth_credentials.has_value()));
    if (use_fallback)
      CreateLoginDelegate();
    else
      RunAuthCredentials(auth_credentials);
  }

  void CreateLoginDelegate() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    login_delegate_ = GetContentClient()->browser()->CreateLoginDelegate(
        auth_info_.get(), web_contents_getter_, request_id_,
        is_request_for_main_frame_, url_, response_headers_,
        first_auth_attempt_,
        base::BindOnce(&LoginHandlerDelegate::RunAuthCredentials,
                       base::Unretained(this)));

    if (!login_delegate_) {
      RunAuthCredentials(base::nullopt);
      return;
    }
  }

  void RunAuthCredentials(
      const base::Optional<net::AuthCredentials>& auth_credentials) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&LoginHandlerDelegate::RunAuthCredentialsOnUI,
                       base::Unretained(this), auth_credentials));
  }

  void RunAuthCredentialsOnUI(
      const base::Optional<net::AuthCredentials>& auth_credentials) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auth_challenge_responder_->OnAuthCredentials(auth_credentials);
    delete this;
  }

  network::mojom::AuthChallengeResponderPtr auth_challenge_responder_;
  scoped_refptr<net::AuthChallengeInfo> auth_info_;
  const content::GlobalRequestID request_id_;
  bool is_request_for_main_frame_;
  GURL url_;
  const scoped_refptr<net::HttpResponseHeaders> response_headers_;
  bool first_auth_attempt_;
  ResourceRequestInfo::WebContentsGetter web_contents_getter_;
  scoped_refptr<LoginDelegate> login_delegate_;
};

void HandleFileUploadRequest(
    uint32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    NetworkServiceClient::OnFileUploadRequestedCallback callback,
    scoped_refptr<base::TaskRunner> task_runner) {
  std::vector<base::File> files;
  uint32_t file_flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
                        (async ? base::File::FLAG_ASYNC : 0);
  ChildProcessSecurityPolicy* cpsp = ChildProcessSecurityPolicy::GetInstance();
  for (const auto& file_path : file_paths) {
    if (process_id != network::mojom::kBrowserProcessId &&
        !cpsp->CanReadFile(process_id, file_path)) {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), net::ERR_ACCESS_DENIED,
                                    std::vector<base::File>()));
      return;
    }
    files.emplace_back(file_path, file_flags);
    if (!files.back().IsValid()) {
      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback),
                         net::FileErrorToNetError(files.back().error_details()),
                         std::vector<base::File>()));
      return;
    }
  }
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), net::OK,
                                                  std::move(files)));
}

}  // namespace

NetworkServiceClient::NetworkServiceClient(
    network::mojom::NetworkServiceClientRequest network_service_client_request)
    : binding_(this, std::move(network_service_client_request))
#if defined(OS_ANDROID)
      ,
      app_status_listener_(base::android::ApplicationStatusListener::New(
          base::BindRepeating(&NetworkServiceClient::OnApplicationStateChange,
                              base::Unretained(this))))
#endif
{
  if (IsOutOfProcessNetworkService())
    net::CertDatabase::GetInstance()->AddObserver(this);
}

NetworkServiceClient::~NetworkServiceClient() {
  if (IsOutOfProcessNetworkService())
    net::CertDatabase::GetInstance()->RemoveObserver(this);
}

void NetworkServiceClient::OnAuthRequired(
    uint32_t process_id,
    uint32_t routing_id,
    uint32_t request_id,
    const GURL& url,
    const GURL& site_for_cookies,
    bool first_auth_attempt,
    const scoped_refptr<net::AuthChallengeInfo>& auth_info,
    int32_t resource_type,
    const base::Optional<network::ResourceResponseHead>& head,
    network::mojom::AuthChallengeResponderPtr auth_challenge_responder) {
  base::Callback<WebContents*(void)> web_contents_getter =
      process_id ? base::Bind(WebContentsImpl::FromRenderFrameHostID,
                              process_id, routing_id)
                 : base::Bind(WebContents::FromFrameTreeNodeId, routing_id);

  if (!web_contents_getter.Run()) {
    std::move(auth_challenge_responder)->OnAuthCredentials(base::nullopt);
    return;
  }

  if (ResourceDispatcherHostImpl::Get()->DoNotPromptForLogin(
          static_cast<ResourceType>(resource_type), url, site_for_cookies)) {
    std::move(auth_challenge_responder)->OnAuthCredentials(base::nullopt);
    return;
  }

  bool is_request_for_main_frame =
      static_cast<ResourceType>(resource_type) == RESOURCE_TYPE_MAIN_FRAME;
  new LoginHandlerDelegate(std::move(auth_challenge_responder),
                           std::move(web_contents_getter), auth_info,
                           is_request_for_main_frame, process_id, routing_id,
                           request_id, url, head ? head->headers : nullptr,
                           first_auth_attempt);  // deletes self
}

void NetworkServiceClient::OnCertificateRequested(
    uint32_t process_id,
    uint32_t routing_id,
    uint32_t request_id,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    network::mojom::NetworkServiceClient::OnCertificateRequestedCallback
        callback) {
  base::Callback<WebContents*(void)> web_contents_getter =
      process_id ? base::Bind(WebContentsImpl::FromRenderFrameHostID,
                              process_id, routing_id)
                 : base::Bind(WebContents::FromFrameTreeNodeId, routing_id);
  if (!web_contents_getter.Run()) {
    network::mojom::SSLPrivateKeyPtr ssl_private_key;
    mojo::MakeRequest(&ssl_private_key);
    std::move(callback).Run(nullptr, std::vector<uint16_t>(),
                            std::move(ssl_private_key),
                            true /* cancel_certificate_selection */);
    return;
  }
  new SSLClientAuthDelegate(std::move(callback), std::move(web_contents_getter),
                            cert_info);  // deletes self
}

void NetworkServiceClient::OnSSLCertificateError(
    uint32_t process_id,
    uint32_t routing_id,
    uint32_t request_id,
    int32_t resource_type,
    const GURL& url,
    const net::SSLInfo& ssl_info,
    bool fatal,
    OnSSLCertificateErrorCallback response) {
  SSLErrorDelegate* delegate =
      new SSLErrorDelegate(std::move(response));  // deletes self
  base::Callback<WebContents*(void)> web_contents_getter =
      process_id ? base::Bind(WebContentsImpl::FromRenderFrameHostID,
                              process_id, routing_id)
                 : base::Bind(WebContents::FromFrameTreeNodeId, routing_id);
  SSLManager::OnSSLCertificateError(
      delegate->GetWeakPtr(), static_cast<ResourceType>(resource_type), url,
      std::move(web_contents_getter), ssl_info, fatal);
}

#if defined(OS_CHROMEOS)
void NetworkServiceClient::OnUsedTrustAnchor(const std::string& username_hash) {
  GetContentClient()->browser()->OnUsedTrustAnchor(username_hash);
}
#endif

void NetworkServiceClient::OnFileUploadRequested(
    uint32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    OnFileUploadRequestedCallback callback) {
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&HandleFileUploadRequest, process_id, async, file_paths,
                     std::move(callback),
                     base::SequencedTaskRunnerHandle::Get()));
}

void NetworkServiceClient::OnCookiesRead(int process_id,
                                         int routing_id,
                                         const GURL& url,
                                         const GURL& first_party_url,
                                         const net::CookieList& cookie_list,
                                         bool blocked_by_policy) {
  GetContentClient()->browser()->OnCookiesRead(process_id, routing_id, url,
                                               first_party_url, cookie_list,
                                               blocked_by_policy);
}

void NetworkServiceClient::OnCookieChange(int process_id,
                                          int routing_id,
                                          const GURL& url,
                                          const GURL& first_party_url,
                                          const net::CanonicalCookie& cookie,
                                          bool blocked_by_policy) {
  GetContentClient()->browser()->OnCookieChange(
      process_id, routing_id, url, first_party_url, cookie, blocked_by_policy);
}

void NetworkServiceClient::OnLoadingStateUpdate(
    std::vector<network::mojom::LoadInfoPtr> infos,
    OnLoadingStateUpdateCallback callback) {
  auto rdh_infos = std::make_unique<ResourceDispatcherHostImpl::LoadInfoList>();

  // TODO(jam): once ResourceDispatcherHost is gone remove the translation
  // (other than adding the WebContents callback).
  for (auto& info : infos) {
    ResourceDispatcherHostImpl::LoadInfo load_info;
    load_info.host = std::move(info->host);
    load_info.load_state.state = static_cast<net::LoadState>(info->load_state);
    load_info.load_state.param = std::move(info->state_param);
    load_info.upload_position = info->upload_position;
    load_info.upload_size = info->upload_size;
    load_info.web_contents_getter =
        info->process_id
            ? base::BindRepeating(WebContentsImpl::FromRenderFrameHostID,
                                  info->process_id, info->routing_id)
            : base::BindRepeating(WebContents::FromFrameTreeNodeId,
                                  info->routing_id);
    rdh_infos->push_back(std::move(load_info));
  }

  auto* rdh = ResourceDispatcherHostImpl::Get();
  ResourceDispatcherHostImpl::UpdateLoadStateOnUI(rdh->loader_delegate_,
                                                  std::move(rdh_infos));

  std::move(callback).Run();
}

void NetworkServiceClient::OnClearSiteData(int process_id,
                                           int routing_id,
                                           const GURL& url,
                                           const std::string& header_value,
                                           int load_flags,
                                           OnClearSiteDataCallback callback) {
  base::RepeatingCallback<WebContents*(void)> web_contents_getter =
      process_id
          ? base::BindRepeating(WebContentsImpl::FromRenderFrameHostID,
                                process_id, routing_id)
          : base::BindRepeating(WebContents::FromFrameTreeNodeId, routing_id);

  ClearSiteDataHandler::HandleHeader(std::move(web_contents_getter), url,
                                     header_value, load_flags,
                                     std::move(callback));
}

void NetworkServiceClient::OnCertDBChanged() {
  GetNetworkService()->OnCertDBChanged();
}

#if defined(OS_ANDROID)
void NetworkServiceClient::OnApplicationStateChange(
    base::android::ApplicationState state) {
  GetNetworkService()->OnApplicationStateChange(state);
}
#endif

void NetworkServiceClient::OnDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {
  GetContentClient()->browser()->OnNetworkServiceDataUseUpdate(
      network_traffic_annotation_id_hash, recv_bytes, sent_bytes);
}

}  // namespace content
