// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/install_prompt_data.h"

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/ui_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {

namespace {

bool AllowWebstoreData(InstallPromptData::PromptType type) {
  return type == InstallPromptData::EXTERNAL_INSTALL_PROMPT ||
         type == InstallPromptData::REPAIR_PROMPT;
}

}  // namespace

InstallPromptData::InstallPromptData(PromptType type) : type_(type) {
  DCHECK_NE(type_, NUM_PROMPT_TYPES);
}

InstallPromptData::~InstallPromptData() = default;

void InstallPromptData::AddPermissionSet(const PermissionSet& permissions) {
  Manifest::Type type =
      extension_ ? extension_->GetType() : Manifest::Type::kUnknown;
  prompt_permissions_.LoadFromPermissionSet(&permissions, type);
  if (!permissions.effective_hosts().is_empty()) {
    is_requesting_host_permissions_ = true;
  }
}

void InstallPromptData::AddPermissionMessages(
    const PermissionMessages& permissions) {
  prompt_permissions_.AddPermissionMessages(permissions);
}

void InstallPromptData::SetWebstoreData(
    const std::string& localized_user_count,
    bool show_user_count,
    double average_rating,
    int rating_count,
    const std::string& localized_rating_count) {
  CHECK(AllowWebstoreData(type_));
  localized_user_count_ = localized_user_count;
  show_user_count_ = show_user_count;
  average_rating_ = average_rating;
  rating_count_ = rating_count;
  localized_rating_count_ = localized_rating_count;
  has_webstore_data_ = true;
}

void InstallPromptData::SetInitialExtensionsProviderName(
    std::u16string initial_extensions_provider_name) {
  initial_extensions_provider_name_ =
      std::move(initial_extensions_provider_name);
}

std::u16string InstallPromptData::GetDialogTitle() const {
  int id = -1;
  switch (type_) {
    case INSTALL_PROMPT:
      id = IDS_EXTENSION_INSTALL_PROMPT_TITLE;
      break;
    case RE_ENABLE_PROMPT:
      id = IDS_EXTENSION_RE_ENABLE_PROMPT_TITLE;
      break;
    case PERMISSIONS_PROMPT:
      id = IDS_EXTENSION_PERMISSIONS_PROMPT_TITLE;
      break;
    case EXTERNAL_INSTALL_PROMPT:
      if (extension_->is_app()) {
        id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_TITLE_APP;
      } else if (extension_->is_theme()) {
        id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_TITLE_THEME;
      } else if (!initial_extensions_provider_name_.empty()) {
        return l10n_util::GetStringFUTF16(
            IDS_EXTENSION_EXTERNAL_INITIAL_INSTALL_PROMPT_TITLE_EXTENSION,
            initial_extensions_provider_name_,
            ui_util::GetFixupExtensionNameForUIDisplay(extension_->name()));
      } else {
        id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_TITLE_EXTENSION;
      }
      break;
    case REMOTE_INSTALL_PROMPT:
      id = IDS_EXTENSION_REMOTE_INSTALL_PROMPT_TITLE;
      break;
    case REPAIR_PROMPT:
      id = IDS_EXTENSION_REPAIR_PROMPT_TITLE;
      break;
    case EXTENSION_REQUEST_PROMPT:
      id = IDS_EXTENSION_REQUEST_PROMPT_TITLE;
      break;
    case EXTENSION_PENDING_REQUEST_PROMPT:
      id = IDS_EXTENSION_PENDING_REQUEST_PROMPT_TITLE;
      break;
    case EXTENSION_PARENT_APPROVAL_PROMPT:
      id = IDS_EXTENSION_PARENT_APPROVAL_PROMPT_TITLE;
      break;
    case UNSET_PROMPT_TYPE:
    case NUM_PROMPT_TYPES:
      NOTREACHED();
  }

  return l10n_util::GetStringFUTF16(
      id, ui_util::GetFixupExtensionNameForUIDisplay(extension_->name()));
}

int InstallPromptData::GetDialogButtons() const {
  DCHECK_NE(type_, UNSET_PROMPT_TYPE);

  // Extension pending request dialog doesn't have confirm button because there
  // is no user action required.
  if (type_ == EXTENSION_PENDING_REQUEST_PROMPT) {
    return static_cast<int>(ui::mojom::DialogButton::kCancel);
  }

  return static_cast<int>(ui::mojom::DialogButton::kOk) |
         static_cast<int>(ui::mojom::DialogButton::kCancel);
}

std::u16string InstallPromptData::GetAcceptButtonLabel() const {
  int id = -1;
  switch (type_) {
    case INSTALL_PROMPT:
      if (requires_parent_permission()) {
        id = IDS_EXTENSION_INSTALL_PROMPT_ASK_A_PARENT_BUTTON;
      } else if (extension_->is_app()) {
        id = IDS_EXTENSION_INSTALL_PROMPT_ACCEPT_BUTTON_APP;
      } else if (extension_->is_theme()) {
        id = IDS_EXTENSION_INSTALL_PROMPT_ACCEPT_BUTTON_THEME;
      } else {
        id = IDS_EXTENSION_INSTALL_PROMPT_ACCEPT_BUTTON_EXTENSION;
      }
      break;
    case RE_ENABLE_PROMPT:
      id = IDS_EXTENSION_PROMPT_RE_ENABLE_BUTTON;
      break;
    case PERMISSIONS_PROMPT:
      id = IDS_EXTENSION_PROMPT_PERMISSIONS_BUTTON;
      break;
    case EXTERNAL_INSTALL_PROMPT:
      if (extension_->is_app()) {
        id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_ACCEPT_BUTTON_APP;
      } else if (extension_->is_theme()) {
        id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_ACCEPT_BUTTON_THEME;
      } else {
        id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_ACCEPT_BUTTON_EXTENSION;
      }
      break;
    case REMOTE_INSTALL_PROMPT:
      if (extension_->is_app()) {
        id = IDS_EXTENSION_PROMPT_REMOTE_INSTALL_BUTTON_APP;
      } else {
        id = IDS_EXTENSION_PROMPT_REMOTE_INSTALL_BUTTON_EXTENSION;
      }
      break;
    case REPAIR_PROMPT:
      if (extension_->is_app()) {
        id = IDS_EXTENSION_PROMPT_REPAIR_BUTTON_APP;
      } else {
        id = IDS_EXTENSION_PROMPT_REPAIR_BUTTON_EXTENSION;
      }
      break;
    case EXTENSION_REQUEST_PROMPT:
      id = IDS_EXTENSION_INSTALL_PROMPT_REQUEST_BUTTON;
      break;
    case EXTENSION_PENDING_REQUEST_PROMPT:
      // Pending request prompt doesn't have accept button.
      break;
    case EXTENSION_PARENT_APPROVAL_PROMPT:
      id = IDS_PARENT_PERMISSION_PROMPT_APPROVE_BUTTON;
      break;
    case UNSET_PROMPT_TYPE:
    case NUM_PROMPT_TYPES:
      NOTREACHED();
  }

  return id != -1 ? l10n_util::GetStringUTF16(id) : std::u16string();
}

std::u16string InstallPromptData::GetAbortButtonLabel() const {
  int id = -1;
  switch (type_) {
    case INSTALL_PROMPT:
    case RE_ENABLE_PROMPT:
    case REMOTE_INSTALL_PROMPT:
    case REPAIR_PROMPT:
    case EXTENSION_REQUEST_PROMPT:
      id = IDS_CANCEL;
      break;
    case PERMISSIONS_PROMPT:
      id = IDS_EXTENSION_PROMPT_PERMISSIONS_ABORT_BUTTON;
      break;
    case EXTERNAL_INSTALL_PROMPT:
      id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_ABORT_BUTTON;
      break;
    case EXTENSION_PENDING_REQUEST_PROMPT:
      id = IDS_CLOSE;
      break;
    case EXTENSION_PARENT_APPROVAL_PROMPT:
      id = IDS_PARENT_PERMISSION_PROMPT_CANCEL_BUTTON;
      break;
    case UNSET_PROMPT_TYPE:
    case NUM_PROMPT_TYPES:
      NOTREACHED();
  }

  return l10n_util::GetStringUTF16(id);
}

std::u16string InstallPromptData::GetPermissionsHeading() const {
  int id = -1;
  switch (type_) {
    case INSTALL_PROMPT:
    case EXTERNAL_INSTALL_PROMPT:
    case REMOTE_INSTALL_PROMPT:
    case EXTENSION_REQUEST_PROMPT:
    case EXTENSION_PENDING_REQUEST_PROMPT:
      id = IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO;
      break;
    case RE_ENABLE_PROMPT:
      id = IDS_EXTENSION_PROMPT_WILL_NOW_HAVE_ACCESS_TO;
      break;
    case PERMISSIONS_PROMPT:
      id = IDS_EXTENSION_PROMPT_WANTS_ACCESS_TO;
      break;
    case REPAIR_PROMPT:
      id = IDS_EXTENSION_PROMPT_CAN_ACCESS;
      break;
    case EXTENSION_PARENT_APPROVAL_PROMPT:
      id = IDS_EXTENSION_PROMPT_REQUESTS_PERMISSIONS;
      break;
    case UNSET_PROMPT_TYPE:
    case NUM_PROMPT_TYPES:
      NOTREACHED();
  }
  return l10n_util::GetStringUTF16(id);
}

std::tuple<int, double> InstallPromptData::GetRatingStars() const {
  CHECK(AllowWebstoreData(type_));

  // The star display logic replicates the one used by the webstore (from
  // components.ratingutils.setFractionalYellowStars).
  int full_stars = floor(average_rating_);
  double rating_fractional = average_rating_ - full_stars;

  if (rating_fractional > 0.66) {
    // Show one more full star (e.g. 3.67 stars is shown as 4 full stars)
    full_stars += 1;
  }

  if (rating_fractional < 0.33 || rating_fractional > 0.66) {
    // Do not show a half star.
    // E.g.:
    //   - 3.32 stars is shown as 3 full stars
    //   - 3.33 stars is shown as 3.5 full stars
    //   - 3.66 stars is shown as 3.5 full stars
    //   - 3.67 stars is shown as 4 full stars
    rating_fractional = 0;
  }

  return {full_stars, rating_fractional};
}

std::u16string InstallPromptData::GetRatingCount() const {
  CHECK(AllowWebstoreData(type_));
  return l10n_util::GetStringFUTF16(IDS_EXTENSION_RATING_COUNT,
                                    base::UTF8ToUTF16(localized_rating_count_));
}

std::u16string InstallPromptData::GetUserCount() const {
  CHECK(AllowWebstoreData(type_));

  if (show_user_count_) {
    return l10n_util::GetStringFUTF16(IDS_EXTENSION_USER_COUNT,
                                      base::UTF8ToUTF16(localized_user_count_));
  }
  return std::u16string();
}

size_t InstallPromptData::GetPermissionCount() const {
  return prompt_permissions_.permissions.size();
}

InstallPromptPermissions InstallPromptData::GetPermissions() const {
  return prompt_permissions_;
}

std::u16string InstallPromptData::GetPermission(size_t index) const {
  CHECK_LT(index, prompt_permissions_.permissions.size());
  return prompt_permissions_.permissions[index];
}

void InstallPromptData::AddObserver(
    ExtensionInstallPromptClient::Observer* observer) {
  observers_.AddObserver(observer);
}

void InstallPromptData::RemoveObserver(
    ExtensionInstallPromptClient::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InstallPromptData::OnDialogOpened() {
  for (auto& observer : observers_) {
    observer.OnDialogOpened();
  }
}

void InstallPromptData::OnDialogAccepted() {
  for (auto& observer : observers_) {
    observer.OnDialogAccepted();
  }
}

void InstallPromptData::OnDialogCanceled() {
  for (auto& observer : observers_) {
    observer.OnDialogCanceled();
  }
}

bool InstallPromptData::ShouldWithheldPermissionsOnDialogAccept() const {
  return base::FeatureList::IsEnabled(
             extensions_features::
                 kAllowWithholdingExtensionPermissionsOnInstall) &&
         util::CanWithholdPermissionsFromExtension(*extension_) &&
         is_requesting_host_permissions_ && type_ == INSTALL_PROMPT;
}

}  // namespace extensions
