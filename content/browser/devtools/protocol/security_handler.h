// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SECURITY_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SECURITY_HANDLER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/security.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class DevToolsAgentHostImpl;
class RenderFrameHostImpl;

namespace protocol {

class SecurityHandler : public DevToolsDomainHandler,
                        public Security::Backend,
                        public WebContentsObserver {
 public:
  using CertErrorCallback =
      base::OnceCallback<void(content::CertificateRequestResultType)>;

  SecurityHandler();

  SecurityHandler(const SecurityHandler&) = delete;
  SecurityHandler& operator=(const SecurityHandler&) = delete;

  ~SecurityHandler() override;

  static std::vector<SecurityHandler*> ForAgentHost(
      DevToolsAgentHostImpl* host);

  // DevToolsDomainHandler overrides
  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;

  // Security::Backend overrides.
  Response Enable() override;
  Response Disable() override;
  Response HandleCertificateError(int event_id, const String& action) override;
  Response SetOverrideCertificateErrors(bool override) override;
  Response SetIgnoreCertificateErrors(bool ignore) override;

  // NotifyCertificateError will send a CertificateError event. Returns true if
  // the error is expected to be handled by a corresponding
  // HandleCertificateError command, and false otherwise.
  bool NotifyCertificateError(int cert_error,
                              const GURL& request_url,
                              CertErrorCallback callback);

  bool IsIgnoreCertificateErrorsSet() const;

 private:
  using CertErrorCallbackMap = base::flat_map<int, CertErrorCallback>;

  void AttachToRenderFrameHost();
  void FlushPendingCertificateErrorNotifications();
  Response AssureTopLevelActiveFrame();

  // WebContentsObserver overrides
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  std::unique_ptr<Security::Frontend> frontend_;
  bool enabled_;
  raw_ptr<RenderFrameHostImpl> host_;
  int last_cert_error_id_ = 0;
  CertErrorCallbackMap cert_error_callbacks_;
  enum class CertErrorOverrideMode { kDisabled, kHandleEvents, kIgnoreAll };
  CertErrorOverrideMode cert_error_override_mode_ =
      CertErrorOverrideMode::kDisabled;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SECURITY_HANDLER_H_
