// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_manager.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/ukm_source_id.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/task/post_task.h"
#include "content/browser/devtools/devtools_instrumentation.h"
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
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace content {

namespace {

const char kSSLManagerKeyName[] = "content_ssl_manager";

// Used to log type of mixed content displayed/ran, matches histogram enum
// (MixedContentType). DO NOT REORDER.
enum class MixedContentType {
  kDisplayMixedContent = 0,
  kDisplayWithCertErrors = 1,
  kMixedForm = 2,
  kScriptingMixedContent = 3,
  kScriptingWithCertErrors = 4,
  kMaxValue = kScriptingWithCertErrors,
};

void OnAllowCertificate(SSLErrorHandler* handler,
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
        state_delegate->AllowCert(
            handler->request_url().host(), *handler->ssl_info().cert.get(),
            handler->cert_error(), handler->web_contents());
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

  std::set<SSLManager*>& get() { return set_; }

 private:
  std::set<SSLManager*> set_;

  DISALLOW_COPY_AND_ASSIGN(SSLManagerSet);
};

void LogMixedContentMetrics(MixedContentType type,
                            ukm::SourceId source_id,
                            ukm::UkmRecorder* recorder) {
  UMA_HISTOGRAM_ENUMERATION("SSL.MixedContentShown", type);
  ukm::builders::SSL_MixedContentShown(source_id)
      .SetType(static_cast<int64_t>(type))
      .Record(recorder);
}

}  // namespace

// static
void SSLManager::OnSSLCertificateError(
    const base::WeakPtr<SSLErrorHandler::Delegate>& delegate,
    bool is_main_frame_request,
    const GURL& url,
    WebContents* web_contents,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DCHECK(delegate.get());
  DVLOG(1) << "OnSSLCertificateError() cert_error: " << net_error
           << " url: " << url.spec() << " cert_status: " << std::hex
           << ssl_info.cert_status;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<SSLErrorHandler> handler(
      new SSLErrorHandler(web_contents, delegate, is_main_frame_request, url,
                          net_error, ssl_info, fatal));

  if (!web_contents) {
    // Requests can fail to dispatch because they don't have a WebContents. See
    // https://crbug.com/86537. In this case we have to make a decision in this
    // function.
    handler->DenyRequest();
    return;
  }

  // Check if we should deny certificate errors using the main frame's URL.
  if (GetContentClient()->browser()->ShouldDenyRequestOnCertificateError(
          web_contents->GetLastCommittedURL())) {
    handler->DenyRequest();
    return;
  }

  NavigationControllerImpl* controller =
      static_cast<NavigationControllerImpl*>(&web_contents->GetController());
  controller->SetPendingNavigationSSLError(true);

  SSLManager* manager = controller->ssl_manager();
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
  } else {
    // For main-frame non-same-page navigations, clear content status
    // flags. These flags are set based on the content on the page, and thus
    // should reflect the current content, even if the navigation was to an
    // existing entry that already had content status flags set.
    remove_content_status_flags = ~0;
    // Also clear any UserData from the SSLStatus.
    if (entry)
      entry->GetSSL().user_data = nullptr;
  }

  if (!UpdateEntry(entry, add_content_status_flags,
                   remove_content_status_flags)) {
    // Ensure the WebContents is notified that the SSL state changed when a
    // load is committed, in case the active navigation entry has changed.
    NotifyDidChangeVisibleSSLState();
  }
}

void SSLManager::DidDisplayMixedContent() {
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();
  if (entry && entry->GetURL().SchemeIsCryptographic() &&
      entry->GetSSL().certificate) {
    WebContentsImpl* contents = static_cast<WebContentsImpl*>(
        controller_->delegate()->GetWebContents());
    ukm::SourceId source_id = contents->GetMainFrame()->GetPageUkmSourceId();
    LogMixedContentMetrics(MixedContentType::kDisplayMixedContent, source_id,
                           ukm::UkmRecorder::Get());
  }
  UpdateLastCommittedEntry(SSLStatus::DISPLAYED_INSECURE_CONTENT, 0);
}

void SSLManager::DidContainInsecureFormAction() {
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();
  if (entry && entry->GetURL().SchemeIsCryptographic() &&
      entry->GetSSL().certificate) {
    WebContentsImpl* contents = static_cast<WebContentsImpl*>(
        controller_->delegate()->GetWebContents());
    ukm::SourceId source_id = contents->GetMainFrame()->GetPageUkmSourceId();
    LogMixedContentMetrics(MixedContentType::kMixedForm, source_id,
                           ukm::UkmRecorder::Get());
  }
  UpdateLastCommittedEntry(SSLStatus::DISPLAYED_FORM_WITH_INSECURE_ACTION, 0);
}

void SSLManager::DidDisplayContentWithCertErrors() {
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();
  if (!entry)
    return;
  // Only record information about subresources with cert errors if the
  // main page is HTTPS with a certificate.
  if (entry->GetURL().SchemeIsCryptographic() && entry->GetSSL().certificate) {
    WebContentsImpl* contents = static_cast<WebContentsImpl*>(
        controller_->delegate()->GetWebContents());
    ukm::SourceId source_id = contents->GetMainFrame()->GetPageUkmSourceId();
    LogMixedContentMetrics(MixedContentType::kDisplayWithCertErrors, source_id,
                           ukm::UkmRecorder::Get());
    UpdateLastCommittedEntry(SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS, 0);
  }
}

void SSLManager::DidRunMixedContent(const GURL& security_origin) {
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();
  if (!entry)
    return;

  if (entry->GetURL().SchemeIsCryptographic() && entry->GetSSL().certificate) {
    WebContentsImpl* contents = static_cast<WebContentsImpl*>(
        controller_->delegate()->GetWebContents());
    ukm::SourceId source_id = contents->GetMainFrame()->GetPageUkmSourceId();
    LogMixedContentMetrics(MixedContentType::kScriptingMixedContent, source_id,
                           ukm::UkmRecorder::Get());
  }

  SiteInstance* site_instance = entry->site_instance();
  if (!site_instance)
    return;

  if (ssl_host_state_delegate_) {
    ssl_host_state_delegate_->HostRanInsecureContent(
        security_origin.host(), site_instance->GetProcess()->GetID(),
        SSLHostStateDelegate::MIXED_CONTENT);
  }
  UpdateEntry(entry, 0, 0);
  NotifySSLInternalStateChanged(controller_->GetBrowserContext());
}

void SSLManager::DidRunContentWithCertErrors(const GURL& security_origin) {
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();
  if (!entry)
    return;

  if (entry->GetURL().SchemeIsCryptographic() && entry->GetSSL().certificate) {
    WebContentsImpl* contents = static_cast<WebContentsImpl*>(
        controller_->delegate()->GetWebContents());
    ukm::SourceId source_id = contents->GetMainFrame()->GetPageUkmSourceId();
    LogMixedContentMetrics(MixedContentType::kScriptingWithCertErrors,
                           source_id, ukm::UkmRecorder::Get());
  }

  SiteInstance* site_instance = entry->site_instance();
  if (!site_instance)
    return;

  if (ssl_host_state_delegate_) {
    ssl_host_state_delegate_->HostRanInsecureContent(
        security_origin.host(), site_instance->GetProcess()->GetID(),
        SSLHostStateDelegate::CERT_ERRORS_CONTENT);
  }
  UpdateEntry(entry, 0, 0);
  NotifySSLInternalStateChanged(controller_->GetBrowserContext());
}

void SSLManager::OnCertError(std::unique_ptr<SSLErrorHandler> handler) {
  // First we check if we know the policy for this error.
  DCHECK(handler->ssl_info().is_valid());
  SSLHostStateDelegate::CertJudgment judgment =
      ssl_host_state_delegate_
          ? ssl_host_state_delegate_->QueryPolicy(
                handler->request_url().host(), *handler->ssl_info().cert.get(),
                handler->cert_error(), handler->web_contents())
          : SSLHostStateDelegate::DENIED;

  if (judgment == SSLHostStateDelegate::ALLOWED) {
    handler->ContinueRequest();
    return;
  }

  DCHECK(net::IsCertificateError(handler->cert_error()));
  OnCertErrorInternal(std::move(handler));
}

void SSLManager::DidStartResourceResponse(const GURL& url,
                                          bool has_certificate_errors) {
  if (!url.SchemeIsCryptographic() || has_certificate_errors)
    return;

  // If the scheme is https: or wss and the cert did not have any errors, revoke
  // any previous decisions that have occurred.
  if (!ssl_host_state_delegate_ ||
      !ssl_host_state_delegate_->HasAllowException(
          url.host(), controller_->GetWebContents())) {
    return;
  }

  // If there's no certificate error, a good certificate has been seen, so
  // clear out any exceptions that were made by the user for bad
  // certificates. This intentionally does not apply to cached resources
  // (see https://crbug.com/634553 for an explanation).
  ssl_host_state_delegate_->RevokeUserAllowExceptions(url.host());
}

void SSLManager::OnCertErrorInternal(std::unique_ptr<SSLErrorHandler> handler) {
  WebContents* web_contents = handler->web_contents();
  int cert_error = handler->cert_error();
  const net::SSLInfo& ssl_info = handler->ssl_info();
  const GURL& request_url = handler->request_url();
  bool is_main_frame_request = handler->is_main_frame_request();
  bool fatal = handler->fatal();

  base::RepeatingCallback<void(bool, content::CertificateRequestResultType)>
      callback = base::BindRepeating(&OnAllowCertificate,
                                     base::Owned(handler.release()),
                                     ssl_host_state_delegate_);

  if (devtools_instrumentation::HandleCertificateError(
          web_contents, cert_error, request_url,
          base::BindRepeating(callback, false))) {
    return;
  }

  GetContentClient()->browser()->AllowCertificateError(
      web_contents, cert_error, ssl_info, request_url, is_main_frame_request,
      fatal, base::BindOnce(std::move(callback), true));
}

bool SSLManager::UpdateEntry(NavigationEntryImpl* entry,
                             int add_content_status_flags,
                             int remove_content_status_flags) {
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
    const base::Optional<url::Origin>& entry_origin =
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
    NotifyDidChangeVisibleSSLState();
    return true;
  }

  return false;
}

void SSLManager::UpdateLastCommittedEntry(int add_content_status_flags,
                                          int remove_content_status_flags) {
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();
  if (!entry)
    return;
  UpdateEntry(entry, add_content_status_flags, remove_content_status_flags);
}

void SSLManager::NotifyDidChangeVisibleSSLState() {
  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(controller_->delegate()->GetWebContents());
  contents->DidChangeVisibleSecurityState();
}

// static
void SSLManager::NotifySSLInternalStateChanged(BrowserContext* context) {
  SSLManagerSet* managers =
      static_cast<SSLManagerSet*>(context->GetUserData(kSSLManagerKeyName));

  for (auto i = managers->get().begin(); i != managers->get().end(); ++i) {
    (*i)->UpdateEntry((*i)->controller()->GetLastCommittedEntry(), 0, 0);
  }
}

}  // namespace content
