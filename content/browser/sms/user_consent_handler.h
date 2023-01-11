// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_USER_CONSENT_HANDLER_H_
#define CONTENT_BROWSER_SMS_USER_CONSENT_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/sms/webotp_service.mojom-shared.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;

enum class UserConsentResult {
  kApproved,
  kDenied,
  kNoDelegate,
  kInactiveRenderFrameHost
};
using CompletionCallback = base::OnceCallback<void(UserConsentResult)>;

class CONTENT_EXPORT UserConsentHandler {
 public:
  virtual ~UserConsentHandler() = default;

  // Ask for the user consent. Once the process is complete it invokes
  // |on_complete| callback with the appropriate status.
  virtual void RequestUserConsent(const std::string& one_time_code,
                                  CompletionCallback on_complete) = 0;

  // Returns true if it is still processing an inflight request.
  // Note that this always returns false for not asynchronous handlers.
  virtual bool is_active() const = 0;

  // Returns true if this handler processes request asynchronously.
  virtual bool is_async() const = 0;
};

class CONTENT_EXPORT NoopUserConsentHandler : public UserConsentHandler {
 public:
  ~NoopUserConsentHandler() override;
  void RequestUserConsent(const std::string& one_time_code,
                          CompletionCallback on_complete) override;
  bool is_active() const override;
  bool is_async() const override;
};

class CONTENT_EXPORT PromptBasedUserConsentHandler : public UserConsentHandler {
 public:
  using OriginList = std::vector<url::Origin>;

  PromptBasedUserConsentHandler(RenderFrameHost& frame_host,
                                const OriginList& origin_list);
  ~PromptBasedUserConsentHandler() override;

  void RequestUserConsent(const std::string& one_time_code,
                          CompletionCallback on_complete) override;
  bool is_active() const override;
  bool is_async() const override;

  void OnConfirm();
  void OnCancel();

 private:
  raw_ref<RenderFrameHost> frame_host_;
  const OriginList origin_list_;
  bool is_prompt_open_{false};
  CompletionCallback on_complete_;

  base::WeakPtrFactory<PromptBasedUserConsentHandler> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_USER_CONSENT_HANDLER_H_
