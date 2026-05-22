// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/bnpl/ui/ios_bnpl_ui_delegate.h"

#import "base/check_deref.h"
#import "base/notimplemented.h"
#import "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#import "components/autofill/core/browser/payments/bnpl_util.h"
#import "components/autofill/core/browser/ui/payments/autofill_progress_ui_type.h"

namespace autofill::payments {

IosBnplUiDelegate::IosBnplUiDelegate(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

IosBnplUiDelegate::~IosBnplUiDelegate() = default;

void IosBnplUiDelegate::ShowSelectBnplIssuerUi(
    std::vector<BnplIssuerContext> bnpl_issuer_context,
    std::string app_locale,
    base::RepeatingCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback,
    bool has_seen_ai_terms) {
  // TODO(crbug.com/469521271): Implement native BNPL issuer selection UI.
  NOTIMPLEMENTED();
}

void IosBnplUiDelegate::UpdateBnplIssuerUi(
    std::vector<BnplIssuerContext> issuer_contexts,
    std::optional<int64_t> extracted_amount,
    bool is_amount_supported_by_any_issuer,
    const std::optional<std::string>& app_locale,
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback) {
  // TODO(crbug.com/469521271): Implement native BNPL issuer UI updates.
  NOTIMPLEMENTED();
}

void IosBnplUiDelegate::RemoveSelectBnplIssuerOrProgressUi() {
  // TODO(crbug.com/469521271): Implement dismissal logic.
  NOTIMPLEMENTED();
}

void IosBnplUiDelegate::ShowBnplTosUi(BnplTosModel bnpl_tos_model,
                                      base::OnceClosure accept_callback,
                                      base::OnceClosure cancel_callback) {
  // TODO(crbug.com/469521271): Implement native BNPL Terms of Service UI.
  NOTIMPLEMENTED();
}

void IosBnplUiDelegate::RemoveBnplTosOrProgressUi() {
  // TODO(crbug.com/469521271): Implement ToS dismissal logic.
  NOTIMPLEMENTED();
}

void IosBnplUiDelegate::ShowProgressUi(
    AutofillProgressUiType autofill_progress_dialog_type,
    base::OnceClosure cancel_callback) {
  // TODO(crbug.com/469521271): Implement progressive loader UI.
  NOTIMPLEMENTED();
}

void IosBnplUiDelegate::CloseProgressUi(bool credit_card_fetched_successfully) {
  // TODO(crbug.com/469521271): Implement progressive loader dismissal.
  NOTIMPLEMENTED();
}

void IosBnplUiDelegate::ShowAutofillErrorUi(
    AutofillErrorDialogContext context) {
  // TODO(crbug.com/469521271): Implement BNPL error dialog presentation.
  NOTIMPLEMENTED();
}

}  // namespace autofill::payments
