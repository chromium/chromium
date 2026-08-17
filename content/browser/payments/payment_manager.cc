// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/payments/payment_app_database.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace {

constexpr char kInvalidPaymentManagerStateMessage[] =
    "Scope URL not set, Init may not have been called.";

enum class ReasonCode : uint32_t {
  kInvalidContextUrl,
  kNonUnicodeScopeString,
  kInvalidScopeUrl,
  kCrossOriginContextAndScope,
  kCrossOriginDataAccess,
  kRenderProcessCannotAccessOrigin,
  kInvalidState,
};

}  // namespace

PaymentManager::PaymentManager(
    PaymentAppContextImpl* payment_app_context,
    const url::Origin& origin,
    mojo::PendingReceiver<payments::mojom::PaymentManager> receiver)
    : payment_app_context_(payment_app_context),
      origin_(origin),
      receiver_(this, std::move(receiver)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(payment_app_context);

  receiver_.set_disconnect_handler(base::BindOnce(
      &PaymentManager::OnConnectionError, weak_ptr_factory_.GetWeakPtr()));
}

PaymentManager::~PaymentManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void PaymentManager::Init(const GURL& context_url, const std::string& scope) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context_url.is_valid()) {
    receiver_.ResetWithReason(
        static_cast<uint32_t>(ReasonCode::kInvalidContextUrl),
        "Invalid context URL.");
    return;
  }
  // GURL constructors CHECKs when the input string is not unicode.
  if (!base::IsStringUTF8(scope)) {
    receiver_.ResetWithReason(
        static_cast<uint32_t>(ReasonCode::kNonUnicodeScopeString),
        "Scope string is not UTF8.");
    return;
  }
  GURL scope_url(scope);
  if (!scope_url.is_valid()) {
    receiver_.ResetWithReason(
        static_cast<uint32_t>(ReasonCode::kInvalidScopeUrl),
        "Invalid scope URL.");
    return;
  }
  if (!url::IsSameOriginWith(context_url, scope_url)) {
    receiver_.ResetWithReason(
        static_cast<uint32_t>(ReasonCode::kCrossOriginContextAndScope),
        "Scope URL is not from the same origin of the context URL.");
    return;
  }
  if (!origin_.IsSameOriginWith(context_url)) {
    receiver_.ResetWithReason(
        static_cast<uint32_t>(ReasonCode::kCrossOriginDataAccess),
        "Cross origin data access.");
    return;
  }

  scope_ = scope_url;
}


void PaymentManager::SetUserHint(const std::string& user_hint) {
  if (scope_.is_empty()) {
    receiver_.ResetWithReason(static_cast<uint32_t>(ReasonCode::kInvalidState),
                              kInvalidPaymentManagerStateMessage);
    return;
  }

  payment_app_context_->payment_app_database()->SetPaymentAppUserHint(
      scope_, user_hint);
}

void PaymentManager::EnableDelegations(
    const std::vector<payments::mojom::PaymentDelegation>& delegations,
    PaymentManager::EnableDelegationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (scope_.is_empty()) {
    receiver_.ResetWithReason(static_cast<uint32_t>(ReasonCode::kInvalidState),
                              kInvalidPaymentManagerStateMessage);
    return;
  }

  payment_app_context_->payment_app_database()->EnablePaymentAppDelegations(
      scope_, delegations, std::move(callback));
}

void PaymentManager::OnConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  payment_app_context_->PaymentManagerHadConnectionError(this);
}

}  // namespace content
