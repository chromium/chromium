// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_message_filter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "base/task/post_task.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_constants.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/cpp/features.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !defined(OS_MACOSX)
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/browser/plugin_service_impl.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/public/browser/plugin_service_filter.h"
#endif

namespace content {

namespace {

void CreateChildFrameOnUI(
    int process_id,
    int parent_routing_id,
    blink::WebTreeScopeType scope,
    const std::string& frame_name,
    const std::string& frame_unique_name,
    bool is_created_by_script,
    const base::UnguessableToken& devtools_frame_token,
    const blink::FramePolicy& frame_policy,
    const FrameOwnerProperties& frame_owner_properties,
    blink::FrameOwnerElementType owner_type,
    int new_routing_id,
    mojo::ScopedMessagePipeHandle interface_provider_request_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(process_id, parent_routing_id);
  // Handles the RenderFrameHost being deleted on the UI thread while
  // processing a subframe creation message.
  if (render_frame_host) {
    render_frame_host->OnCreateChildFrame(
        new_routing_id,
        service_manager::mojom::InterfaceProviderRequest(
            std::move(interface_provider_request_handle)),
        scope, frame_name, frame_unique_name, is_created_by_script,
        devtools_frame_token, frame_policy, frame_owner_properties, owner_type);
  }
}

// |blob_data_handle| is only here for the legacy code path. With network
// service enabled |blob_url_token| should be provided and will be used instead
// to download the correct blob.
void DownloadUrlOnUIThread(
    std::unique_ptr<download::DownloadUrlParameters> parameters,
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle,
    blink::mojom::BlobURLTokenPtrInfo blob_url_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(parameters->render_process_host_id());
  if (!render_process_host)
    return;

  BrowserContext* browser_context = render_process_host->GetBrowserContext();

  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (blob_url_token) {
    blob_url_loader_factory =
        ChromeBlobStorageContext::URLLoaderFactoryForToken(
            browser_context,
            blink::mojom::BlobURLTokenPtr(std::move(blob_url_token)));
  }

  DownloadManager* download_manager =
      BrowserContext::GetDownloadManager(browser_context);
  parameters->set_download_source(download::DownloadSource::FROM_RENDERER);
  download_manager->DownloadUrl(std::move(parameters),
                                std::move(blob_data_handle),
                                std::move(blob_url_loader_factory));
}

// With network service disabled the downloads code wouldn't know what to do
// with a BlobURLToken, so this method is used to convert from a token to a
// BlobDataHandle to be passed on to the rest of the downloads system.
void DownloadBlobURLFromToken(
    std::unique_ptr<download::DownloadUrlParameters> params,
    blink::mojom::BlobURLTokenPtr,
    const base::WeakPtr<storage::BlobStorageContext>& context,
    const base::UnguessableToken& token) {
  std::unique_ptr<storage::BlobDataHandle> blob_handle;
  GURL blob_url;
  if (context) {
    std::string uuid;
    if (context->registry().GetTokenMapping(token, &blob_url, &uuid) &&
        blob_url == params->url()) {
      blob_handle = context->GetBlobDataFromUUID(uuid);
    }
  }
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&DownloadUrlOnUIThread, std::move(params),
                     std::move(blob_handle), nullptr));
}

// Common functionality for converting a sync renderer message to a callback
// function in the browser. Derive from this, create it on the heap when
// issuing your callback. When done, write your reply parameters into
// reply_msg(), and then call SendReplyAndDeleteThis().
class RenderMessageCompletionCallback {
 public:
  RenderMessageCompletionCallback(RenderFrameMessageFilter* filter,
                                  IPC::Message* reply_msg)
      : filter_(filter),
        reply_msg_(reply_msg) {
  }

  virtual ~RenderMessageCompletionCallback() {
    if (reply_msg_) {
      // If the owner of this class failed to call SendReplyAndDeleteThis(),
      // send an error reply to prevent the renderer from being hung.
      reply_msg_->set_reply_error();
      filter_->Send(reply_msg_);
    }
  }

  RenderFrameMessageFilter* filter() { return filter_.get(); }
  IPC::Message* reply_msg() { return reply_msg_; }

  void SendReplyAndDeleteThis() {
    filter_->Send(reply_msg_);
    reply_msg_ = nullptr;
    delete this;
  }

 private:
  scoped_refptr<RenderFrameMessageFilter> filter_;
  IPC::Message* reply_msg_;
};

}  // namespace

#if BUILDFLAG(ENABLE_PLUGINS)

class RenderFrameMessageFilter::OpenChannelToPpapiBrokerCallback
    : public PpapiPluginProcessHost::BrokerClient {
 public:
  OpenChannelToPpapiBrokerCallback(RenderFrameMessageFilter* filter,
                                   int routing_id)
      : filter_(filter),
        routing_id_(routing_id) {
  }

  ~OpenChannelToPpapiBrokerCallback() override {}

  void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                           int* renderer_id) override {
    // base::kNullProcessHandle indicates that the channel will be used by the
    // browser itself. Make sure we never output that value here.
    CHECK_NE(base::kNullProcessHandle, filter_->PeerHandle());
    *renderer_handle = filter_->PeerHandle();
    *renderer_id = filter_->render_process_id_;
  }

  void OnPpapiChannelOpened(const IPC::ChannelHandle& channel_handle,
                            base::ProcessId plugin_pid,
                            int /* plugin_child_id */) override {
    filter_->Send(new ViewMsg_PpapiBrokerChannelCreated(routing_id_,
                                                        plugin_pid,
                                                        channel_handle));
    delete this;
  }

  bool Incognito() override { return filter_->incognito_; }

 private:
  scoped_refptr<RenderFrameMessageFilter> filter_;
  int routing_id_;
};

class RenderFrameMessageFilter::OpenChannelToPpapiPluginCallback
    : public RenderMessageCompletionCallback,
      public PpapiPluginProcessHost::PluginClient {
 public:
  OpenChannelToPpapiPluginCallback(RenderFrameMessageFilter* filter,
                                   IPC::Message* reply_msg)
      : RenderMessageCompletionCallback(filter, reply_msg) {}

  void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                           int* renderer_id) override {
    // base::kNullProcessHandle indicates that the channel will be used by the
    // browser itself. Make sure we never output that value here.
    CHECK_NE(base::kNullProcessHandle, filter()->PeerHandle());
    *renderer_handle = filter()->PeerHandle();
    *renderer_id = filter()->render_process_id_;
  }

  void OnPpapiChannelOpened(const IPC::ChannelHandle& channel_handle,
                            base::ProcessId plugin_pid,
                            int plugin_child_id) override {
    FrameHostMsg_OpenChannelToPepperPlugin::WriteReplyParams(
        reply_msg(), channel_handle, plugin_pid, plugin_child_id);
    SendReplyAndDeleteThis();
  }

  bool Incognito() override { return filter()->incognito_; }
};

#endif  // ENABLE_PLUGINS

RenderFrameMessageFilter::RenderFrameMessageFilter(
    int render_process_id,
    PluginServiceImpl* plugin_service,
    BrowserContext* browser_context,
    StoragePartition* storage_partition,
    RenderWidgetHelper* render_widget_helper)
    : BrowserMessageFilter(FrameMsgStart),
      BrowserAssociatedInterface<mojom::RenderFrameMessageFilter>(this, this),
#if BUILDFLAG(ENABLE_PLUGINS)
      plugin_service_(plugin_service),
      profile_data_directory_(storage_partition->GetPath()),
#endif  // ENABLE_PLUGINS
      request_context_(storage_partition->GetURLRequestContext()),
      resource_context_(browser_context->GetResourceContext()),
      render_widget_helper_(render_widget_helper),
      incognito_(browser_context->IsOffTheRecord()),
      render_process_id_(render_process_id) {
}

RenderFrameMessageFilter::~RenderFrameMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

network::mojom::CookieManagerPtr* RenderFrameMessageFilter::GetCookieManager() {
  if (!cookie_manager_ || cookie_manager_.encountered_error()) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&RenderFrameMessageFilter::InitializeCookieManager, this,
                       mojo::MakeRequest(&cookie_manager_)));
  }
  return &cookie_manager_;
}

void RenderFrameMessageFilter::InitializeCookieManager(
    network::mojom::CookieManagerRequest cookie_manager_request) {
  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(render_process_id_);
  if (!render_process_host)
    return;

  render_process_host->GetStoragePartition()
      ->GetNetworkContext()
      ->GetCookieManager(std::move(cookie_manager_request));
}

bool RenderFrameMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameMessageFilter, message)
    IPC_MESSAGE_HANDLER(FrameHostMsg_CreateChildFrame, OnCreateChildFrame)
    IPC_MESSAGE_HANDLER(FrameHostMsg_CookiesEnabled, OnCookiesEnabled)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DownloadUrl, OnDownloadUrl)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SaveImageFromDataURL,
                        OnSaveImageFromDataURL)
    IPC_MESSAGE_HANDLER(FrameHostMsg_Are3DAPIsBlocked, OnAre3DAPIsBlocked)
    IPC_MESSAGE_HANDLER_GENERIC(FrameHostMsg_RenderProcessGone,
                                OnRenderProcessGone())
#if BUILDFLAG(ENABLE_PLUGINS)
    IPC_MESSAGE_HANDLER(FrameHostMsg_GetPluginInfo, OnGetPluginInfo)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(FrameHostMsg_OpenChannelToPepperPlugin,
                                    OnOpenChannelToPepperPlugin)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidCreateOutOfProcessPepperInstance,
                        OnDidCreateOutOfProcessPepperInstance)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidDeleteOutOfProcessPepperInstance,
                        OnDidDeleteOutOfProcessPepperInstance)
    IPC_MESSAGE_HANDLER(FrameHostMsg_OpenChannelToPpapiBroker,
                        OnOpenChannelToPpapiBroker)
    IPC_MESSAGE_HANDLER(FrameHostMsg_PluginInstanceThrottleStateChange,
                        OnPluginInstanceThrottleStateChange)
#endif  // ENABLE_PLUGINS
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void RenderFrameMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void RenderFrameMessageFilter::DownloadUrl(
    int render_view_id,
    int render_frame_id,
    const GURL& url,
    const Referrer& referrer,
    const url::Origin& initiator,
    const base::string16& suggested_name,
    const bool use_prompt,
    const bool follow_cross_origin_redirects,
    blink::mojom::BlobURLTokenPtrInfo blob_url_token) const {
  if (!resource_context_)
    return;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("renderer_initiated_download", R"(
        semantics {
          sender: "Download from Renderer"
          description:
            "The frame has either navigated to a URL that was determined to be "
            "a download via one of the renderer's classification mechanisms, "
            "or WebView has requested a <canvas> or <img> element at a "
            "specific location be to downloaded."
          trigger:
            "The user navigated to a destination that was categorized as a "
            "download, or WebView triggered saving a <canvas> or <img> tag."
          data: "Only the URL we are attempting to download."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            DownloadRestrictions {
              DownloadRestrictions: 3
            }
          }
        })");
  std::unique_ptr<download::DownloadUrlParameters> parameters(
      new download::DownloadUrlParameters(url, render_process_id_,
                                          render_view_id, render_frame_id,
                                          traffic_annotation));
  parameters->set_content_initiated(true);
  parameters->set_suggested_name(suggested_name);
  parameters->set_prompt(use_prompt);
  parameters->set_follow_cross_origin_redirects(follow_cross_origin_redirects);
  parameters->set_referrer(referrer.url);
  parameters->set_referrer_policy(
      Referrer::ReferrerPolicyForUrlRequest(referrer.policy));
  parameters->set_initiator(initiator);

  // If network service is enabled we should always have a |blob_url_token|,
  // which will be used to download the correct blob. But in the legacy
  // non-network service code path we still need to look up the BlobDataHandle
  // for the URL here, to make sure the correct blob ends up getting downloaded.
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle;
  if (url.SchemeIsBlob()) {
    ChromeBlobStorageContext* blob_context =
        GetChromeBlobStorageContextForResourceContext(resource_context_);

    // With network service disabled the downloads code wouldn't know what to do
    // with the BlobURLToken (or the resulting URLLoaderFactory). So for that
    // case convert the token to a BlobDataHandle before passing it of to the
    // rest of the downloads system.
    if (blob_url_token &&
        !base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      blink::mojom::BlobURLTokenPtr blob_url_token_ptr(
          std::move(blob_url_token));
      auto* raw_token = blob_url_token_ptr.get();
      raw_token->GetToken(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&DownloadBlobURLFromToken, std::move(parameters),
                         std::move(blob_url_token_ptr),
                         blob_context->context()->AsWeakPtr()),
          base::UnguessableToken()));
      return;
    }

    blob_data_handle = blob_context->context()->GetBlobDataFromPublicURL(url);
    // Don't care if the above fails. We are going to let the download go
    // through and allow it to be interrupted so that the embedder can deal.
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&DownloadUrlOnUIThread, std::move(parameters),
                     std::move(blob_data_handle), std::move(blob_url_token)));
}

void RenderFrameMessageFilter::OnCreateChildFrame(
    const FrameHostMsg_CreateChildFrame_Params& params,
    int* new_routing_id,
    mojo::MessagePipeHandle* new_interface_provider,
    base::UnguessableToken* devtools_frame_token) {
  *new_routing_id = render_widget_helper_->GetNextRoutingID();

  service_manager::mojom::InterfaceProviderPtr interface_provider;
  auto interface_provider_request(mojo::MakeRequest(&interface_provider));
  *new_interface_provider =
      interface_provider.PassInterface().PassHandle().release();

  *devtools_frame_token = base::UnguessableToken::Create();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&CreateChildFrameOnUI, render_process_id_,
                     params.parent_routing_id, params.scope, params.frame_name,
                     params.frame_unique_name, params.is_created_by_script,
                     *devtools_frame_token, params.frame_policy,
                     params.frame_owner_properties,
                     params.frame_owner_element_type, *new_routing_id,
                     interface_provider_request.PassMessagePipe()));
}

void RenderFrameMessageFilter::OnCookiesEnabled(int render_frame_id,
                                                const GURL& url,
                                                const GURL& site_for_cookies,
                                                bool* cookies_enabled) {
  // TODO(ananta): If this render frame is associated with an automation
  // channel, aka ChromeFrame then we need to retrieve cookie settings from the
  // external host.
  *cookies_enabled = GetContentClient()->browser()->AllowGetCookie(
      url, site_for_cookies, net::CookieList(), resource_context_,
      render_process_id_, render_frame_id);
}

void RenderFrameMessageFilter::CheckPolicyForCookies(
    int render_frame_id,
    const GURL& url,
    const GURL& site_for_cookies,
    GetCookiesCallback callback,
    const net::CookieList& cookie_list) {
  // Check the policy for get cookies, and pass cookie_list to the
  // TabSpecificContentSetting for logging purpose.
  if (GetContentClient()->browser()->AllowGetCookie(
          url, site_for_cookies, cookie_list, resource_context_,
          render_process_id_, render_frame_id)) {
    std::move(callback).Run(net::CanonicalCookie::BuildCookieLine(cookie_list));
  } else {
    std::move(callback).Run(std::string());
  }
}

void RenderFrameMessageFilter::OnDownloadUrl(
    const FrameHostMsg_DownloadUrl_Params& params) {
  mojo::ScopedMessagePipeHandle blob_url_token_handle(params.blob_url_token);
  blink::mojom::BlobURLTokenPtrInfo blob_url_token(
      std::move(blob_url_token_handle), blink::mojom::BlobURLToken::Version_);
  if (blob_url_token && !params.url.SchemeIsBlob()) {
    bad_message::ReceivedBadMessage(
        this, bad_message::RFMF_BLOB_URL_TOKEN_FOR_NON_BLOB_URL);
    return;
  }

  DownloadUrl(params.render_view_id, params.render_frame_id, params.url,
              params.referrer, params.initiator_origin, params.suggested_name,
              false, params.follow_cross_origin_redirects,
              std::move(blob_url_token));
}

void RenderFrameMessageFilter::OnSaveImageFromDataURL(
    int render_view_id,
    int render_frame_id,
    const std::string& url_str) {
  // Please refer to RenderFrameImpl::saveImageFromDataURL().
  if (url_str.length() >= kMaxLengthOfDataURLString)
    return;

  GURL data_url(url_str);
  if (!data_url.is_valid() || !data_url.SchemeIs(url::kDataScheme))
    return;

  DownloadUrl(render_view_id, render_frame_id, data_url, Referrer(),
              url::Origin(), base::string16(), true, true, nullptr);
}

void RenderFrameMessageFilter::OnAre3DAPIsBlocked(int render_frame_id,
                                                  const GURL& top_origin_url,
                                                  ThreeDAPIType requester,
                                                  bool* blocked) {
  *blocked = GpuDataManagerImpl::GetInstance()->Are3DAPIsBlocked(
      top_origin_url, render_process_id_, render_frame_id, requester);
}

void RenderFrameMessageFilter::OnRenderProcessGone() {
  // FrameHostMessage_RenderProcessGone is a synthetic IPC message used by
  // RenderProcessHostImpl to clean things up after a crash (it's injected
  // downstream of this filter). Allowing it to proceed would enable a renderer
  // to fake its own death; instead, actually kill the renderer.
  bad_message::ReceivedBadMessage(
      this, bad_message::RFMF_RENDERER_FAKED_ITS_OWN_DEATH);
}

void RenderFrameMessageFilter::SetCookie(int32_t render_frame_id,
                                         const GURL& url,
                                         const GURL& site_for_cookies,
                                         const std::string& cookie_line,
                                         SetCookieCallback callback) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessDataForOrigin(render_process_id_, url)) {
    bad_message::BadMessageReason reason =
        bad_message::RFMF_SET_COOKIE_BAD_ORIGIN;
    SYSLOG(WARNING) << "Killing renderer: illegal cookie write. Reason: "
                    << reason;
    bad_message::ReceivedBadMessage(this, reason);
    std::move(callback).Run();
    return;
  }

  net::CookieOptions options;
  std::unique_ptr<net::CanonicalCookie> cookie = net::CanonicalCookie::Create(
      url, cookie_line, base::Time::Now(), options);
  if (!cookie) {
    std::move(callback).Run();
    return;
  }

  if (!GetContentClient()->browser()->AllowSetCookie(
          url, site_for_cookies, *cookie, resource_context_, render_process_id_,
          render_frame_id)) {
    std::move(callback).Run();
    return;
  }

  // If the embedder overrides the cookie store then always use it, even if
  // the network service is enabled, instead of the CookieManager associated
  // this process' StoragePartition.
  net::CookieStore* cookie_store =
      GetContentClient()->browser()->OverrideCookieStoreForURL(
          url, resource_context_);
  if (cookie_store ||
      !base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    std::move(callback).Run();
    if (!cookie_store)
      cookie_store = request_context_->GetURLRequestContext()->cookie_store();

    // Pass a null callback since we don't care about when the 'set' completes.
    cookie_store->SetCanonicalCookieAsync(
        std::move(cookie), url.SchemeIsCryptographic(),
        !options.exclude_httponly(), net::CookieStore::SetCookiesCallback());
    return;
  }

  net::CookieStore::SetCookiesCallback net_callback =
      base::BindOnce([](SetCookieCallback callback,
                        bool success) { std::move(callback).Run(); },
                     std::move(callback));
  (*GetCookieManager())
      ->SetCanonicalCookie(*cookie, url.SchemeIsCryptographic(),
                           !options.exclude_httponly(),
                           std::move(net_callback));
}

void RenderFrameMessageFilter::GetCookies(int render_frame_id,
                                          const GURL& url,
                                          const GURL& site_for_cookies,
                                          GetCookiesCallback callback) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessDataForOrigin(render_process_id_, url)) {
    bad_message::BadMessageReason reason =
        bad_message::RFMF_GET_COOKIES_BAD_ORIGIN;
    SYSLOG(WARNING) << "Killing renderer: illegal cookie read. Reason: "
                    << reason;
    bad_message::ReceivedBadMessage(this, reason);
    std::move(callback).Run(std::string());
    return;
  }

  net::CookieOptions options;
  if (net::registry_controlled_domains::SameDomainOrHost(
          url, site_for_cookies,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    // TODO(mkwst): This check ought to further distinguish between frames
    // initiated in a strict or lax same-site context.
    options.set_same_site_cookie_mode(
        net::CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
  } else {
    options.set_same_site_cookie_mode(
        net::CookieOptions::SameSiteCookieMode::DO_NOT_INCLUDE);
  }
  // If the embedder overrides the cookie store then always use it, even if
  // the network service is enabled, instead of the CookieManager associated
  // this process' StoragePartition.
  net::CookieStore* cookie_store =
      GetContentClient()->browser()->OverrideCookieStoreForURL(
          url, resource_context_);
  if (cookie_store ||
      !base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    if (!cookie_store)
      cookie_store = request_context_->GetURLRequestContext()->cookie_store();

    // If we crash here, figure out what URL the renderer was requesting.
    // http://crbug.com/99242
    DEBUG_ALIAS_FOR_GURL(url_buf, url);

    cookie_store->GetCookieListWithOptionsAsync(
        url, options,
        base::BindOnce(&RenderFrameMessageFilter::CheckPolicyForCookies, this,
                       render_frame_id, url, site_for_cookies,
                       std::move(callback)));
    return;
  }

  (*GetCookieManager())
      ->GetCookieList(
          url, options,
          base::BindOnce(&RenderFrameMessageFilter::CheckPolicyForCookies, this,
                         render_frame_id, url, site_for_cookies,
                         std::move(callback)));
}

#if BUILDFLAG(ENABLE_PLUGINS)

void RenderFrameMessageFilter::OnGetPluginInfo(
    int render_frame_id,
    const GURL& url,
    const url::Origin& main_frame_origin,
    const std::string& mime_type,
    bool* found,
    WebPluginInfo* info,
    std::string* actual_mime_type) {
  bool allow_wildcard = true;
  *found = plugin_service_->GetPluginInfo(
      render_process_id_, render_frame_id, resource_context_, url,
      main_frame_origin, mime_type, allow_wildcard, nullptr, info,
      actual_mime_type);
}

void RenderFrameMessageFilter::OnOpenChannelToPepperPlugin(
    const base::FilePath& path,
    const base::Optional<url::Origin>& origin_lock,
    IPC::Message* reply_msg) {
  plugin_service_->OpenChannelToPpapiPlugin(
      render_process_id_, path, profile_data_directory_, origin_lock,
      new OpenChannelToPpapiPluginCallback(this, reply_msg));
}

void RenderFrameMessageFilter::OnDidCreateOutOfProcessPepperInstance(
    int plugin_child_id,
    int32_t pp_instance,
    PepperRendererInstanceData instance_data,
    bool is_external) {
  // It's important that we supply the render process ID ourselves based on the
  // channel the message arrived on. We use the
  //   PP_Instance -> (process id, frame id)
  // mapping to decide how to handle messages received from the (untrusted)
  // plugin, so an exploited renderer must not be able to insert fake mappings
  // that may allow it access to other render processes.
  DCHECK_EQ(0, instance_data.render_process_id);
  instance_data.render_process_id = render_process_id_;
  if (is_external) {
    // We provide the BrowserPpapiHost to the embedder, so it's safe to cast.
    BrowserPpapiHostImpl* host = static_cast<BrowserPpapiHostImpl*>(
        GetContentClient()->browser()->GetExternalBrowserPpapiHost(
            plugin_child_id));
    if (host)
      host->AddInstance(pp_instance, instance_data);
  } else {
    PpapiPluginProcessHost::DidCreateOutOfProcessInstance(
        plugin_child_id, pp_instance, instance_data);
  }
}

void RenderFrameMessageFilter::OnDidDeleteOutOfProcessPepperInstance(
    int plugin_child_id,
    int32_t pp_instance,
    bool is_external) {
  if (is_external) {
    // We provide the BrowserPpapiHost to the embedder, so it's safe to cast.
    BrowserPpapiHostImpl* host = static_cast<BrowserPpapiHostImpl*>(
        GetContentClient()->browser()->GetExternalBrowserPpapiHost(
            plugin_child_id));
    if (host)
      host->DeleteInstance(pp_instance);
  } else {
    PpapiPluginProcessHost::DidDeleteOutOfProcessInstance(
        plugin_child_id, pp_instance);
  }
}

void RenderFrameMessageFilter::OnOpenChannelToPpapiBroker(
    int routing_id,
    const base::FilePath& path) {
  plugin_service_->OpenChannelToPpapiBroker(
      render_process_id_, routing_id, path,
      new OpenChannelToPpapiBrokerCallback(this, routing_id));
}

void RenderFrameMessageFilter::OnPluginInstanceThrottleStateChange(
    int plugin_child_id,
    int32_t pp_instance,
    bool is_throttled) {
  // Feature is only implemented for non-external Plugins.
  PpapiPluginProcessHost::OnPluginInstanceThrottleStateChange(
      plugin_child_id, pp_instance, is_throttled);
}

#endif  // ENABLE_PLUGINS

}  // namespace content
