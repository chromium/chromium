// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_manager.h"

#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/trace_event/optional_trace_event.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/navigation_or_document_handle.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "net/base/url_util.h"
#include "net/cert/cert_status_flags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace content {

namespace {

const char kSSLManagerKeyName[] = "content_ssl_manager";

// Used to log type of mixed content displayed/ran, matches histogram enum
// (MixedContentType). DO NOT REORDER.
enum class MixedContentType {
  kOptionallyBlockableMixedContent = 0,
  kOptionallyBlockableWithCertErrors = 1,
  kMixedForm = 2,
  kBlockableMixedContent = 3,
  kBlockableWithCertErrors = 4,
  kMaxValue = kBlockableWithCertErrors,
};

void OnAllowCertificate(SSLErrorHandler* handler,
                        StoragePartition* storage_partition,
                        SSLHostStateDelegate* state_delegate,
                        bool record_decision,
                        CertificateRequestResultType decision) {
  DCHECK(handler->ssl_info().is_valid());
  switch (decision) {
    case CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE:
      // Note that we should not call SetMaxSecurityStyle here, because
      // the active NavigationEntry has just been deleted (in
      // HideInterstitialPage) and the new NavigationEntry will not be
      // set until DidNavigate.  This is ok, because the new
      // NavigationEntry will have its max security style set within
      // DidNavigate.
      //
      // While AllowCert() executes synchronously on this thread,
      // ContinueRequest() gets posted to a different thread. Calling
      // AllowCert() first ensures deterministic ordering.
      if (record_decision && state_delegate) {
        state_delegate->AllowCert(handler->request_url().host(),
                                  *handler->ssl_info().cert.get(),
                                  handler->cert_error(), storage_partition);
      }
      handler->ContinueRequest();
      return;
    case CERTIFICATE_REQUEST_RESULT_TYPE_DENY:
      handler->DenyRequest();
      return;
    case CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL:
      handler->CancelRequest();
      return;
  }
}

class SSLManagerSet : public base::SupportsUserData::Data {
 public:
  SSLManagerSet() {
  }

  SSLManagerSet(const SSLManagerSet&) = delete;
  SSLManagerSet& operator=(const SSLManagerSet&) = delete;

  std::set<raw_ptr<SSLManager, SetExperimental>>& get() { return set_; }

 private:
  std::set<raw_ptr<SSLManager, SetExperimental>> set_;
};

}  // namespace

// static
void SSLManager::OnSSLCertificateError(
    const base::WeakPtr<SSLErrorHandler::Delegate>& delegate,
    bool is_primary_main_frame_request,
    const GURL& url,
    NavigationOrDocumentHandle* navigation_or_document,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DCHECK(delegate.get());
  DVLOG(1) << "OnSSLCertificateError() cert_error: " << net_error
           << " url: " << url.spec() << " cert_status: " << std::hex
           << ssl_info.cert_status;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  WebContents* web_contents = nullptr;
  FrameTreeNode* frame_tree_node = nullptr;
  // This handle can be null if the request is from service worker.
  if (navigation_or_document) {
    web_contents = navigation_or_document->GetWebContents();
    frame_tree_node = navigation_or_document->GetFrameTreeNode();
  }

  std::unique_ptr<SSLErrorHandler> handler(
      new SSLErrorHandler(web_contents, delegate, is_primary_main_frame_request,
                          url, net_error, ssl_info, fatal));

  if (!web_contents || !frame_tree_node) {
    // Check if the DevTools Browser target is set to ignore certificate errors.
    if (devtools_instrumentation::ShouldBypassCertificateErrors()) {
      handler->ContinueRequest();
      return;
    }
    // Requests can fail to dispatch because they don't have a WebContents. See
    // https://crbug.com/86537. In this case we have to make a decision in this
    // function. Also, if the navigation or document which have been responsible
    // for the request don't exist, there is no point in trying to process
    // further.
    handler->DenyRequest();
    return;
  }

  // Check if we should deny certificate errors using the main frame's URL.
  if (GetContentClient()->browser()->ShouldDenyRequestOnCertificateError(
          web_contents->GetLastCommittedURL())) {
    handler->DenyRequest();
    return;
  }

  NavigationControllerImpl& controller =
      frame_tree_node->navigator().controller();
  controller.SetPendingNavigationSSLError(true);

  SSLManager* manager = controller.ssl_manager();
  manager->OnCertError(std::move(handler));
}

SSLManager::SSLManager(NavigationControllerImpl* controller)
    : controller_(controller),
      ssl_host_state_delegate_(
          controller->GetBrowserContext()->GetSSLHostStateDelegate()) {
  DCHECK(controller_);

  SSLManagerSet* managers = static_cast<SSLManagerSet*>(
      controller_->GetBrowserContext()->GetUserData(kSSLManagerKeyName));
  if (!managers) {
    auto managers_owned = std::make_unique<SSLManagerSet>();
    managers = managers_owned.get();
    controller_->GetBrowserContext()->SetUserData(kSSLManagerKeyName,
                                                  std::move(managers_owned));
  }
  managers->get().insert(this);
}

SSLManager::~SSLManager() {
  SSLManagerSet* managers = static_cast<SSLManagerSet*>(
      controller_->GetBrowserContext()->GetUserData(kSSLManagerKeyName));
  managers->get().erase(this);
}

void SSLManager::DidCommitProvisionalLoad(const LoadCommittedDetails& details) {
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();
  int add_content_status_flags = 0;
  int remove_content_status_flags = 0;

  if (!details.is_main_frame || details.is_same_document) {
    // For subframe navigations, and for same-document main-frame navigations,
    // carry over content status flags from the previously committed entry. For
    // example, the mixed content flag shouldn't clear because of a subframe
    // navigation, or because of a back/forward navigation that doesn't leave
    // the current document. (See https://crbug.com/959571.)
    NavigationEntryImpl* previous_entry =
        controller_->GetEntryAtIndex(details.previous_entry_index);
    if (previous_entry) {
      add_content_status_flags = previous_entry->GetSSL().content_status;
    }
  } else if (!details.is_prerender_activation) {
    // For main-frame navigations that are not same-document and not prerender
    // activations, clear content status flags. These flags are set based on the
    // content on the page, and thus should reflect the current content, even if
    // the navigation was to an existing entry that already had content status
    // flags set. The status flags are kept for prerender activations because
    // |entry| points to the NavigationEntry that has just committed and it may
    // contain existing ssl flags which we do not want to reset.
    remove_content_status_flags = ~0;
  }

  if (!UpdateEntry(entry, add_content_status_flags, remove_content_status_flags,
                   /*notify_changes=*/details.is_in_active_page)) {
    // Ensure the WebContents is notified that the SSL state changed when a
    // load is committed, in case the active navigation entry has changed.
    // Notification will only be called during activation if this commit is
    // triggered by prerendering.
    if (details.is_in_active_page) {
      NotifyDidChangeVisibleSSLState();
    }
  }
}

void SSLManager::DidDisplayMixedContent() {
  OPTIONAL_TRACE_EVENT0("content", "SSLManager::DidDisplayMixedContent");
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();
  if (entry && entry->GetURL().SchemeIsCryptographic() &&
      entry->GetSSL().certificate) {
    RenderFrameHostImpl* main_frame = controller_->frame_tree().GetMainFrame();
    WebContents* contents = WebContents::FromRenderFrameHost(main_frame);
    if (contents) {
      GetContentClient()->browser()->OnDisplayInsecureContent(contents);
    }
  }
  UpdateLastCommittedEntry(SSLStatus::DISPLAYED_INSECURE_CONTENT, 0);
}

void SSLManager::DidContainInsecureFormAction() {
  OPTIONAL_TRACE_EVENT0("content", "SSLManager::DidContainInsecureFormAction");
  UpdateLastCommittedEntry(SSLStatus::DISPLAYED_FORM_WITH_INSECURE_ACTION, 0);
}

void SSLManager::DidDisplayContentWithCertErrors() {
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();
  if (!entry)
    return;

  if (entry->GetURL().SchemeIsCryptographic() && entry->GetSSL().certificate) {
    UpdateLastCommittedEntry(SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS, 0);
  }
}

void SSLManager::DidRunMixedContent(const GURL& security_origin) {
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();
  if (!entry)
    return;

  SiteInstance* site_instance = entry->site_instance();
  if (!site_instance)
    return;

  if (ssl_host_state_delegate_) {
    ssl_host_state_delegate_->HostRanInsecureContent(
        security_origin.host(), site_instance->GetProcess()->GetID(),
        SSLHostStateDelegate::MIXED_CONTENT);
  }
  // TODO(crbug.com/40223471): Ensure proper notify_changes is passed to
  // UpdateEntry.
  UpdateEntry(entry, 0, 0, /*notify_changes=*/true);
  NotifySSLInternalStateChanged(controller_->GetBrowserContext());
}

void SSLManager::DidRunContentWithCertErrors(const GURL& security_origin) {
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();
  if (!entry)
    return;

  SiteInstance* site_instance = entry->site_instance();
  if (!site_instance)
    return;

  if (ssl_host_state_delegate_) {
    ssl_host_state_delegate_->HostRanInsecureContent(
        security_origin.host(), site_instance->GetProcess()->GetID(),
        SSLHostStateDelegate::CERT_ERRORS_CONTENT);
  }
  // TODO(crbug.com/40223471): Ensure proper notify_changes is passed to
  // UpdateEntry.
  UpdateEntry(entry, 0, 0, /*notify_changes=*/true);
  NotifySSLInternalStateChanged(controller_->GetBrowserContext());
}

void SSLManager::OnCertError(std::unique_ptr<SSLErrorHandler> handler) {
  // First we check if we know the policy for this error.
  DCHECK(handler->ssl_info().is_valid());

  SSLHostStateDelegate::CertJudgment judgment;
  if (net::IsLocalhost(handler->request_url()) &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowInsecureLocalhost)) {
    // If the appropriate flag is set, let requests on localhost go
    // through even if there are certificate errors. Errors on localhost
    // are unlikely to indicate actual security problems.
    judgment = SSLHostStateDelegate::ALLOWED;
  } else if (ssl_host_state_delegate_) {
    judgment = ssl_host_state_delegate_->QueryPolicy(
        handler->request_url().host(), *handler->ssl_info().cert.get(),
        handler->cert_error(),
        controller_->frame_tree().GetMainFrame()->GetStoragePartition());
  } else {
    judgment = SSLHostStateDelegate::DENIED;
  }

  if (judgment == SSLHostStateDelegate::ALLOWED) {
    handler->ContinueRequest();
    return;
  }

  DCHECK(net::IsCertificateError(handler->cert_error()));
  OnCertErrorInternal(std::move(handler));
}

bool SSLManager::HasAllowExceptionForAnyHost() {
  if (!ssl_host_state_delegate_) {
    return false;
  }
  return ssl_host_state_delegate_->HasAllowExceptionForAnyHost(
      controller_->frame_tree().GetMainFrame()->GetStoragePartition());
}

bool SSLManager::DidStartResourceResponse(
    const url::SchemeHostPort& final_response_url,
    bool has_certificate_errors) {
  const std::string& scheme = final_response_url.scheme();
  const std::string& host = final_response_url.host();

  if (!GURL::SchemeIsCryptographic(scheme) || has_certificate_errors) {
    return false;
  }
  // If the scheme is https: or wss and the cert did not have any errors, revoke
  // any previous decisions that have occurred.
  if (!ssl_host_state_delegate_ ||
      !ssl_host_state_delegate_->HasAllowException(
          host,
          controller_->frame_tree().GetMainFrame()->GetStoragePartition())) {
    return false;
  }

  // If there's no certificate error, a good certificate has been seen, so
  // clear out any exceptions that were made by the user for bad
  // certificates. This intentionally does not apply to cached resources
  // (see https://crbug.com/634553 for an explanation).
  ssl_host_state_delegate_->RevokeUserAllowExceptions(host);
  return true;
}

void SSLManager::OnCertErrorInternal(std::unique_ptr<SSLErrorHandler> handler) {
  WebContents* web_contents = handler->web_contents();
  int cert_error = handler->cert_error();
  const net::SSLInfo& ssl_info = handler->ssl_info();
  const GURL& request_url = handler->request_url();
  bool is_primary_main_frame_request = handler->is_primary_main_frame_request();
  bool fatal = handler->fatal();

  base::RepeatingCallback<void(bool, content::CertificateRequestResultType)>
      callback = base::BindRepeating(
          &OnAllowCertificate, base::Owned(handler.release()),
          controller_->frame_tree().GetMainFrame()->GetStoragePartition(),
          ssl_host_state_delegate_);

  if (devtools_instrumentation::HandleCertificateError(
          web_contents, cert_error, request_url,
          base::BindRepeating(callback, false))) {
    return;
  }

  GetContentClient()->browser()->AllowCertificateError(
      web_contents, cert_error, ssl_info, request_url,
      is_primary_main_frame_request, fatal,
      base::BindOnce(std::move(callback), true));
}

bool SSLManager::UpdateEntry(NavigationEntryImpl* entry,
                             int add_content_status_flags,
                             int remove_content_status_flags,
                             bool notify_changes) {
  // We don't always have a navigation entry to update, for example in the
  // case of the Web Inspector.
  if (!entry)
    return false;

  SSLStatus original_ssl_status = entry->GetSSL();  // Copy!
  entry->GetSSL().initialized = true;
  entry->GetSSL().content_status &= ~remove_content_status_flags;
  entry->GetSSL().content_status |= add_content_status_flags;

  SiteInstance* site_instance = entry->site_instance();
  // Note that |site_instance| can be NULL here because NavigationEntries don't
  // necessarily have site instances.  Without a process, the entry can't
  // possibly have insecure content.  See bug https://crbug.com/12423.
  if (site_instance && ssl_host_state_delegate_) {
    const std::optional<url::Origin>& entry_origin =
        entry->root_node()->frame_entry->committed_origin();
    // In some cases (e.g., unreachable URLs), navigation entries might not have
    // origins attached to them. We don't care about tracking mixed content for
    // those cases.
    if (entry_origin.has_value()) {
      const std::string& host = entry_origin->host();
      int process_id = site_instance->GetProcess()->GetID();
      if (ssl_host_state_delegate_->DidHostRunInsecureContent(
              host, process_id, SSLHostStateDelegate::MIXED_CONTENT)) {
        entry->GetSSL().content_status |= SSLStatus::RAN_INSECURE_CONTENT;
      }

      // Only record information about subresources with cert errors if the
      // main page is HTTPS with a certificate.
      if (entry->GetURL().SchemeIsCryptographic() &&
          entry->GetSSL().certificate &&
          ssl_host_state_delegate_->DidHostRunInsecureContent(
              host, process_id, SSLHostStateDelegate::CERT_ERRORS_CONTENT)) {
        entry->GetSSL().content_status |=
            SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS;
      }
    }
  }

  if (entry->GetSSL().initialized != original_ssl_status.initialized ||
      entry->GetSSL().content_status != original_ssl_status.content_status) {
    if (notify_changes) {
      NotifyDidChangeVisibleSSLState();
    }
    return true;
  }

  return false;
}

void SSLManager::UpdateLastCommittedEntry(int add_content_status_flags,
                                          int remove_content_status_flags) {
  NavigationEntryImpl* entry;
  if (controller_->frame_tree().is_fenced_frame()) {
    // Only the primary frame tree's NavigationEntries are exposed outside of
    // content, so the primary frame tree's NavigationController needs to
    // represent an aggregate view of the security state of its inner frame
    // trees.
    RenderFrameHostImpl* rfh =
        controller_->frame_tree().root()->current_frame_host();
    DCHECK(rfh);
    CHECK_NE(RenderFrameHostImpl::LifecycleStateImpl::kPrerendering,
             rfh->GetOutermostMainFrame()->lifecycle_state());
    WebContentsImpl* contents =
        WebContentsImpl::FromRenderFrameHostImpl(rfh->GetOutermostMainFrame());
    entry = contents->GetController().GetLastCommittedEntry();
  } else {
    entry = controller_->GetLastCommittedEntry();
  }

  if (!entry)
    return;
  // TODO(crbug.com/40223471): Ensure proper notify_changes is passed to
  // UpdateEntry.
  UpdateEntry(entry, add_content_status_flags, remove_content_status_flags,
              /*notify_changes=*/true);
}

void SSLManager::NotifyDidChangeVisibleSSLState() {
  RenderFrameHostImpl* main_frame = controller_->frame_tree().GetMainFrame();
  WebContentsImpl* contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(main_frame));
  contents->DidChangeVisibleSecurityState();
}

// static
void SSLManager::NotifySSLInternalStateChanged(BrowserContext* context) {
  SSLManagerSet* managers =
      static_cast<SSLManagerSet*>(context->GetUserData(kSSLManagerKeyName));

  for (SSLManager* manager : managers->get()) {
    // TODO(crbug.com/40223471): Ensure proper notify_changes is passed to
    // UpdateEntry.
    manager->UpdateEntry(manager->controller()->GetLastCommittedEntry(), 0, 0,
                         /*notify_changes=*/true);
  }
}

}  // namespace content
