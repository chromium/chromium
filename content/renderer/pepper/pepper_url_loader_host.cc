// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_url_loader_host.h"

#include <stddef.h>

#include "base/feature_list.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "content/public/common/content_features.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "content/renderer/pepper/url_request_info_util.h"
#include "content/renderer/pepper/url_response_info_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"

using blink::WebAssociatedURLLoader;
using blink::WebAssociatedURLLoaderOptions;
using blink::WebLocalFrame;
using blink::WebString;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebURLResponse;

#ifdef _MSC_VER
// Do not warn about use of std::copy with raw pointers.
#pragma warning(disable : 4996)
#endif

namespace content {

PepperURLLoaderHost::PepperURLLoaderHost(RendererPpapiHostImpl* host,
                                         bool main_document_loader,
                                         PP_Instance instance,
                                         PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      main_document_loader_(main_document_loader),
      has_universal_access_(false),
      bytes_sent_(0),
      total_bytes_to_be_sent_(-1),
      bytes_received_(0),
      total_bytes_to_be_received_(-1),
      pending_response_(false) {
  DCHECK((main_document_loader && !resource) ||
         (!main_document_loader && resource));
}

PepperURLLoaderHost::~PepperURLLoaderHost() {
  // Normally deleting this object will delete the loader which will implicitly
  // cancel the load. But this won't happen for the main document loader. So it
  // would be nice to issue a Close() here.
  //
  // However, the PDF plugin will cancel the document load and then close the
  // resource (which is reasonable). It then makes a second request to load the
  // document so it can set the "want progress" flags (which is unreasonable --
  // we should probably provide download progress on document loads).
  //
  // But a Close() on the main document (even if the request is already
  // canceled) will cancel all pending subresources, of which the second
  // request is one, and the load will fail. Even if we fixed the PDF reader to
  // change the timing or to send progress events to avoid the second request,
  // we don't want to cancel other loads when the main one is closed.
  //
  // "Leaking" the main document load here by not closing it will only affect
  // plugins handling main document loads (which are very few, mostly only PDF)
  // that dereference without explicitly closing the main document load (which
  // PDF doesn't do -- it explicitly closes it before issuing the second
  // request). And the worst thing that will happen is that any remaining data
  // will get queued inside WebKit.
  if (main_document_loader_) {
    // The PluginInstance has a non-owning pointer to us.
    PepperPluginInstanceImpl* instance_object =
        renderer_ppapi_host_->GetPluginInstanceImpl(pp_instance());
    if (instance_object) {
      DCHECK(instance_object->document_loader() == this);
      instance_object->set_document_loader(nullptr);
    }
  }

  // There is a path whereby the destructor for the loader_ member can
  // invoke InstanceWasDeleted() upon this URLLoaderResource, thereby
  // re-entering the scoped_ptr destructor with the same scoped_ptr object
  // via loader_.reset(). Be sure that loader_ is first NULL then destroy
  // the scoped_ptr. See http://crbug.com/159429.
  std::unique_ptr<WebAssociatedURLLoader> for_destruction_only(
      loader_.release());
}

int32_t PepperURLLoaderHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperURLLoaderHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_URLLoader_Open,
                                      OnHostMsgOpen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_URLLoader_SetDeferLoading,
                                      OnHostMsgSetDeferLoading)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_URLLoader_Close,
                                        OnHostMsgClose);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_URLLoader_GrantUniversalAccess,
        OnHostMsgGrantUniversalAccess)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

bool PepperURLLoaderHost::WillFollowRedirect(
    const WebURL& new_url,
    const WebURLResponse& redirect_response) {
  DCHECK(out_of_order_replies_.empty());
  if (base::FeatureList::IsEnabled(
          features::kPepperCrossOriginRedirectRestriction)) {
    // Follows the Firefox approach
    // (https://bugzilla.mozilla.org/show_bug.cgi?id=1436241) to disallow
    // cross-origin 307/308 POST redirects for requests from plugins. But we try
    // allowing only GET and HEAD methods rather than disallowing POST.
    // See http://crbug.com/332023 for details.
    int status = redirect_response.HttpStatusCode();
    if ((status == 307 || status == 308)) {
      std::string method = base::ToUpperASCII(request_data_.method);
      // method can be an empty string for default behavior, GET.
      if (!method.empty() && method != net::HttpRequestHeaders::kGetMethod &&
          method != net::HttpRequestHeaders::kHeadMethod) {
        return false;
      }
    }
  }

  if (!request_data_.follow_redirects) {
    SaveResponse(redirect_response);
    SetDefersLoading(true);
    // Defer the request and wait the plugin to audit the redirect. We
    // shouldn't return false here as decision has been delegated to the
    // plugin.
  }
  return true;
}

void PepperURLLoaderHost::DidSendData(uint64_t bytes_sent,
                                      uint64_t total_bytes_to_be_sent) {
  // TODO(darin): Bounds check input?
  bytes_sent_ = static_cast<int64_t>(bytes_sent);
  total_bytes_to_be_sent_ = static_cast<int64_t>(total_bytes_to_be_sent);
  UpdateProgress();
}

void PepperURLLoaderHost::DidReceiveResponse(const WebURLResponse& response) {
  // Sets -1 if the content length is unknown. Send before issuing callback.
  total_bytes_to_be_received_ = response.ExpectedContentLength();
  UpdateProgress();

  SaveResponse(response);
}

void PepperURLLoaderHost::DidDownloadData(uint64_t data_length) {
  bytes_received_ += data_length;
  UpdateProgress();
}

void PepperURLLoaderHost::DidReceiveData(base::span<const char> data) {
  // Note that |loader| will be NULL for document loads.
  bytes_received_ += data.size();
  UpdateProgress();

  auto message = std::make_unique<PpapiPluginMsg_URLLoader_SendData>();
  message->WriteData(data.data(), data.size());
  SendUpdateToPlugin(std::move(message));
}

void PepperURLLoaderHost::DidFinishLoading() {
  // Note that |loader| will be NULL for document loads.
  SendUpdateToPlugin(
      std::make_unique<PpapiPluginMsg_URLLoader_FinishedLoading>(PP_OK));
}

void PepperURLLoaderHost::DidFail(const WebURLError& error) {
  // Note that |loader| will be NULL for document loads.
  int32_t pp_error = PP_ERROR_FAILED;
  // TODO(bbudge): Extend pp_errors.h to cover interesting network errors
  // from the net error domain.
  switch (error.reason()) {
    case net::ERR_ACCESS_DENIED:
    case net::ERR_NETWORK_ACCESS_DENIED:
      pp_error = PP_ERROR_NOACCESS;
      break;
  }

  if (error.is_web_security_violation())
    pp_error = PP_ERROR_NOACCESS;
  SendUpdateToPlugin(
      std::make_unique<PpapiPluginMsg_URLLoader_FinishedLoading>(pp_error));
}

void PepperURLLoaderHost::DidConnectPendingHostToResource() {
  for (const auto& reply : pending_replies_)
    host()->SendUnsolicitedReply(pp_resource(), *reply);
  pending_replies_.clear();
}

int32_t PepperURLLoaderHost::OnHostMsgOpen(
    ppapi::host::HostMessageContext* context,
    const ppapi::URLRequestInfoData& request_data) {
  // An "Open" isn't a resource Call so has no reply, but failure to open
  // implies a load failure. To make it harder to forget to send the load
  // failed reply from the open handler, we instead catch errors and convert
  // them to load failed messages.
  int32_t ret = InternalOnHostMsgOpen(context, request_data);
  DCHECK(ret != PP_OK_COMPLETIONPENDING);

  if (ret != PP_OK)
    SendUpdateToPlugin(
        std::make_unique<PpapiPluginMsg_URLLoader_FinishedLoading>(ret));
  return PP_OK;
}

// Since this is wrapped by OnHostMsgOpen, we can return errors here and they
// will be translated into a FinishedLoading call automatically.
int32_t PepperURLLoaderHost::InternalOnHostMsgOpen(
    ppapi::host::HostMessageContext* context,
    const ppapi::URLRequestInfoData& request_data) {
  // Main document loads are already open, so don't allow people to open them
  // again.
  if (main_document_loader_)
    return PP_ERROR_INPROGRESS;

  // Create a copy of the request data since CreateWebURLRequest will populate
  // the file refs.
  ppapi::URLRequestInfoData filled_in_request_data = request_data;

  if (URLRequestRequiresUniversalAccess(filled_in_request_data) &&
      !has_universal_access_) {
    ppapi::PpapiGlobals::Get()->LogWithSource(
        pp_instance(),
        PP_LOGLEVEL_ERROR,
        std::string(),
        "PPB_URLLoader.Open: The URL you're requesting is "
        " on a different security origin than your plugin. To request "
        " cross-origin resources, see "
        " PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS.");
    return PP_ERROR_NOACCESS;
  }

  if (loader_.get())
    return PP_ERROR_INPROGRESS;

  WebLocalFrame* frame = GetFrame();
  if (!frame)
    return PP_ERROR_FAILED;

  WebURLRequest web_request;
  if (!CreateWebURLRequest(
          pp_instance(), &filled_in_request_data, frame, &web_request)) {
    return PP_ERROR_FAILED;
  }

  // Requests from plug-ins must be marked as PLUGIN, and must skip service
  // workers, see the comment in CreateWebURLRequest.
  DCHECK_EQ(blink::mojom::RequestContextType::PLUGIN,
            web_request.GetRequestContext());
  DCHECK(web_request.GetSkipServiceWorker());

  WebAssociatedURLLoaderOptions options;
  if (has_universal_access_) {
    options.grant_universal_access = true;
  } else {
    // All other HTTP requests are untrusted.
    options.untrusted_http = true;
    if (filled_in_request_data.allow_cross_origin_requests) {
      // Allow cross-origin requests with access control. The request specifies
      // if credentials are to be sent.
      web_request.SetMode(network::mojom::RequestMode::kCors);
      web_request.SetCredentialsMode(
          filled_in_request_data.allow_credentials
              ? network::mojom::CredentialsMode::kInclude
              : network::mojom::CredentialsMode::kOmit);
    } else {
      web_request.SetMode(network::mojom::RequestMode::kSameOrigin);
      // Same-origin requests can always send credentials. Use the default
      // credentials mode "include".
    }
  }

  loader_ = frame->CreateAssociatedURLLoader(options);
  if (!loader_.get())
    return PP_ERROR_FAILED;

  // Don't actually save the request until we know we're going to load.
  request_data_ = filled_in_request_data;
  loader_->LoadAsynchronously(web_request, this);

  // Although the request is technically pending, this is not a "Call" message
  // so we don't return COMPLETIONPENDING.
  return PP_OK;
}

int32_t PepperURLLoaderHost::OnHostMsgSetDeferLoading(
    ppapi::host::HostMessageContext* context,
    bool defers_loading) {
  SetDefersLoading(defers_loading);
  return PP_OK;
}

int32_t PepperURLLoaderHost::OnHostMsgClose(
    ppapi::host::HostMessageContext* context) {
  Close();
  return PP_OK;
}

int32_t PepperURLLoaderHost::OnHostMsgGrantUniversalAccess(
    ppapi::host::HostMessageContext* context) {
  // Only plugins with permission can bypass same origin.
  if (host()->permissions().HasPermission(ppapi::PERMISSION_PDF)) {
    has_universal_access_ = true;
    return PP_OK;
  }
  return PP_ERROR_FAILED;
}

void PepperURLLoaderHost::SendUpdateToPlugin(
    std::unique_ptr<IPC::Message> message) {
  // We must send messages to the plugin in the order that the responses are
  // received from webkit, even when the host isn't ready to send messages or
  // when the host performs an asynchronous operation.
  //
  // Only {FinishedLoading, ReceivedResponse, SendData} have ordering
  // contraints; all other messages are immediately added to pending_replies_.
  //
  // Accepted orderings for {FinishedLoading, ReceivedResponse, SendData} are:
  //   - {ReceivedResponse, SendData (zero or more times), FinishedLoading}
  //   - {FinishedLoading (when status != PP_OK)}
  if (message->type() == PpapiPluginMsg_URLLoader_SendData::ID ||
      message->type() == PpapiPluginMsg_URLLoader_FinishedLoading::ID) {
    // Messages that must be sent after ReceivedResponse.
    if (pending_response_) {
      out_of_order_replies_.push_back(std::move(message));
    } else {
      SendOrderedUpdateToPlugin(std::move(message));
    }
  } else if (message->type() == PpapiPluginMsg_URLLoader_ReceivedResponse::ID) {
    // Allow SendData and FinishedLoading into the ordered queue.
    DCHECK(pending_response_);
    SendOrderedUpdateToPlugin(std::move(message));
    for (auto& reply : out_of_order_replies_)
      SendOrderedUpdateToPlugin(std::move(reply));
    out_of_order_replies_.clear();
    pending_response_ = false;
  } else {
    // Messages without ordering constraints.
    SendOrderedUpdateToPlugin(std::move(message));
  }
}

void PepperURLLoaderHost::SendOrderedUpdateToPlugin(
    std::unique_ptr<IPC::Message> message) {
  if (pp_resource() == 0) {
    pending_replies_.push_back(std::move(message));
  } else {
    host()->SendUnsolicitedReply(pp_resource(), *message);
  }
}

void PepperURLLoaderHost::Close() {
  if (loader_.get()) {
    loader_->Cancel();
  } else if (main_document_loader_) {
    // TODO(raymes): Calling WebLocalFrame::stopLoading here is incorrect as it
    // cancels all URL loaders associated with the frame. If a client has opened
    // other URLLoaders and then closes the main one, the others should still
    // remain connected. Work out how to only cancel the main request:
    // crbug.com/384197.
    WebLocalFrame* frame = GetFrame();
    if (frame)
      frame->DeprecatedStopLoading();
  }
}

WebLocalFrame* PepperURLLoaderHost::GetFrame() {
  PepperPluginInstanceImpl* instance_object =
      static_cast<PepperPluginInstanceImpl*>(
          renderer_ppapi_host_->GetPluginInstance(pp_instance()));
  if (!instance_object || instance_object->is_deleted())
    return nullptr;
  return instance_object->GetContainer()->GetDocument().GetFrame();
}

void PepperURLLoaderHost::SetDefersLoading(bool defers_loading) {
  if (loader_.get())
    loader_->SetDefersLoading(defers_loading);

  // TODO(brettw) bug 96770: We need a way to set the defers loading flag on
  // main document loads (when the loader_ is null).
}

void PepperURLLoaderHost::SaveResponse(const WebURLResponse& response) {
  // When we're the main document loader, we send the response data up front,
  // so we don't want to trigger any callbacks in the plugin which aren't
  // expected. We should not be getting redirects so the response sent
  // up-front should be valid (plugin document loads happen after all
  // redirects are processed since WebKit has to know the MIME type).
  if (!main_document_loader_) {
    // We note when there's a callback in flight for a response to ensure that
    // messages we send to the plugin are not sent out of order. See
    // SendUpdateToPlugin() for more details.
    DCHECK(!pending_response_);
    pending_response_ = true;

    SendUpdateToPlugin(
        std::make_unique<PpapiPluginMsg_URLLoader_ReceivedResponse>(
            DataFromWebURLResponse(response)));
  }
}

void PepperURLLoaderHost::UpdateProgress() {
  bool record_download = request_data_.record_download_progress;
  bool record_upload = request_data_.record_upload_progress;
  if (record_download || record_upload) {
    // Here we go through some effort to only send the exact information that
    // the requestor wanted in the request flags. It would be just as
    // efficient to send all of it, but we don't want people to rely on
    // getting download progress when they happen to set the upload progress
    // flag.
    ppapi::proxy::ResourceMessageReplyParams params;
    SendUpdateToPlugin(
        std::make_unique<PpapiPluginMsg_URLLoader_UpdateProgress>(
            record_upload ? bytes_sent_ : -1,
            record_upload ? total_bytes_to_be_sent_ : -1,
            record_download ? bytes_received_ : -1,
            record_download ? total_bytes_to_be_received_ : -1));
  }
}

}  // namespace content
