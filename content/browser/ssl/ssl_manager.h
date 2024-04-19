// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_MANAGER_H_
#define CONTENT_BROWSER_SSL_SSL_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/ssl_status.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {
class SSLInfo;
}

namespace content {
class BrowserContext;
class NavigationEntryImpl;
class NavigationControllerImpl;
class NavigationOrDocumentHandle;
class SSLHostStateDelegate;
struct LoadCommittedDetails;

// The SSLManager controls the SSL UI elements in a WebContents.  It
// listens for various events that influence when these elements should or
// should not be displayed and adjusts them accordingly.
//
// There is one SSLManager per tab.
// The security state (secure/insecure) is stored in the navigation entry.
// Along with it are stored any SSL error code and the associated cert.
class SSLManager {
 public:
  // Entry point for SSLCertificateErrors.  This function begins the process
  // of resolving a certificate error during an SSL connection.  SSLManager
  // will adjust the security UI and either call |CancelSSLRequest| or
  // |ContinueSSLRequest| of |delegate|. |is_primary_main_frame_request| is true
  // only when the request is for a navigation in the primary main frame.
  //
  // This can be called on the UI or IO thread. It will call |delegate| on the
  // same thread.
  static void OnSSLCertificateError(
      const base::WeakPtr<SSLErrorHandler::Delegate>& delegate,
      bool is_primary_main_frame_request,
      const GURL& url,
      NavigationOrDocumentHandle* navigation_or_document,
      int net_error,
      const net::SSLInfo& ssl_info,
      bool fatal);

  // Construct an SSLManager for the specified tab.
  explicit SSLManager(NavigationControllerImpl* controller);

  SSLManager(const SSLManager&) = delete;
  SSLManager& operator=(const SSLManager&) = delete;

  virtual ~SSLManager();

  // The navigation controller associated with this SSLManager.  The
  // NavigationController is guaranteed to outlive the SSLManager.
  NavigationControllerImpl* controller() { return controller_; }

  void DidCommitProvisionalLoad(const LoadCommittedDetails& details);

  // TODO(crbug.com/40879220): Revert function DidStartResourceResponse to
  // return void after expiry of histogram SSL.Experimental.SubresourceResponse.
  // Return true when a good certificate is seen and any exceptions that were
  // made by the user for bad certificates are cleared out, returns false
  // otherwise without processing anything.
  bool DidStartResourceResponse(const url::SchemeHostPort& final_response_url,
                                bool has_certificate_errors);

  // The following methods are called when a page includes insecure
  // content. These methods update the SSLStatus on the NavigationEntry
  // appropriately. If the result could change the visible SSL state,
  // they notify the WebContents of the change via
  // DidChangeVisibleSecurityState();
  // These methods are not called for resource preloads.
  void DidDisplayMixedContent();
  void DidContainInsecureFormAction();
  void DidDisplayContentWithCertErrors();
  void DidRunMixedContent(const GURL& security_origin);
  void DidRunContentWithCertErrors(const GURL& security_origin);

  // An error occurred with the certificate in an SSL connection.
  void OnCertError(std::unique_ptr<SSLErrorHandler> handler);

  // Returns true if any HTTPS-related warning exceptions has been allowed by
  // the user for any host.
  bool HasAllowExceptionForAnyHost();

 private:
  // Helper method for handling certificate errors.
  void OnCertErrorInternal(std::unique_ptr<SSLErrorHandler> handler);

  // Updates the NavigationEntry's |content_status| flags according to state in
  // |ssl_host_state_delegate|, and calls NotifyDidChangeVisibleSSLState
  // according to |notify_changes|. |add_content_status_flags| and
  // |remove_content_status_flags| are bitmasks of SSLStatus::ContentStatusFlags
  // that will be added or removed from the |content_status| field. (Pass 0 to
  // add/remove no content status flags.) |remove_content_status_flags| are
  // removed before |add_content_status_flags| are added. If the final set of
  // flags changes, this method will notify the WebContents and return true.
  bool UpdateEntry(NavigationEntryImpl* entry,
                   int add_content_status_flags,
                   int remove_content_status_flags,
                   bool notify_changes);

  // Helper function for UpdateEntry().
  void UpdateLastCommittedEntry(int add_content_status_flags,
                                int remove_content_status_flags);

  // Notifies the WebContents that the SSL state changed.
  void NotifyDidChangeVisibleSSLState();

  // Updates the last committed entries of all |context|'s
  // SSLManagers. Notifies each WebContents of visible SSL state changes
  // if necessary.
  static void NotifySSLInternalStateChanged(BrowserContext* context);

  // The NavigationController that owns this SSLManager.  We are responsible
  // for the security UI of this tab.
  raw_ptr<NavigationControllerImpl> controller_;

  // Delegate that manages SSL state specific to each host.
  raw_ptr<SSLHostStateDelegate> ssl_host_state_delegate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_SSL_MANAGER_H_
