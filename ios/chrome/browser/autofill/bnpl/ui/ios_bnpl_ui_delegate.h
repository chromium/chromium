// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_IOS_BNPL_UI_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_IOS_BNPL_UI_DELEGATE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/ui/payments/bnpl_ui_delegate.h"

namespace autofill {

struct AutofillErrorDialogContext;
enum class AutofillProgressUiType;
class BnplIssuer;
class AutofillClient;

namespace payments {

struct BnplIssuerContext;
struct BnplTosModel;

// iOS implementation of the BnplUiDelegate interface.
// This class handles the UI for the BNPL autofill flow on the iOS platform.
class IosBnplUiDelegate : public BnplUiDelegate {
 public:
  explicit IosBnplUiDelegate(AutofillClient* client);
  IosBnplUiDelegate(const IosBnplUiDelegate&) = delete;
  IosBnplUiDelegate& operator=(const IosBnplUiDelegate&) = delete;
  ~IosBnplUiDelegate() override;

  // BnplUiDelegate:
  void ShowSelectBnplIssuerUi(
      std::vector<BnplIssuerContext> bnpl_issuer_context,
      std::string app_locale,
      base::RepeatingCallback<void(BnplIssuer)> selected_issuer_callback,
      base::OnceClosure cancel_callback,
      bool has_seen_ai_terms) override;
  void UpdateBnplIssuerUi(
      std::vector<BnplIssuerContext> issuer_contexts,
      std::optional<int64_t> extracted_amount,
      bool is_amount_supported_by_any_issuer,
      const std::optional<std::string>& app_locale,
      base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
      base::OnceClosure cancel_callback) override;
  void RemoveSelectBnplIssuerOrProgressUi() override;
  void ShowBnplTosUi(BnplTosModel bnpl_tos_model,
                     base::OnceClosure accept_callback,
                     base::OnceClosure cancel_callback) override;
  void RemoveBnplTosOrProgressUi() override;
  void ShowProgressUi(AutofillProgressUiType autofill_progress_dialog_type,
                      base::OnceClosure cancel_callback) override;
  void CloseProgressUi(bool credit_card_fetched_successfully) override;
  void ShowAutofillErrorUi(AutofillErrorDialogContext context) override;

 private:
  const raw_ref<AutofillClient> client_;
};

}  // namespace payments
}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_IOS_BNPL_UI_DELEGATE_H_
