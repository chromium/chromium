// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_INSTALL_PROMPT_DATA_H_
#define EXTENSIONS_BROWSER_INSTALL_PROMPT_DATA_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "extensions/browser/extension_install_prompt_client.h"
#include "extensions/browser/install_prompt_permissions.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_message.h"
#include "ui/gfx/image/image.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
class PermissionSet;

// Extra information needed to display an installation or uninstallation
// prompt. Gets populated with raw data and exposes getters for formatted
// strings so that the GTK/views/Cocoa install dialogs don't have to repeat
// that logic.
class InstallPromptData {
 public:
  // This enum is associated with Extensions.InstallPrompt_Type UMA histogram.
  // Do not modify existing values and add new values only to the end.
  enum PromptType {
    UNSET_PROMPT_TYPE = -1,
    INSTALL_PROMPT = 0,
    // INLINE_INSTALL_PROMPT_DEPRECATED = 1,
    // BUNDLE_INSTALL_PROMPT_DEPRECATED = 2,
    RE_ENABLE_PROMPT = 3,
    PERMISSIONS_PROMPT = 4,
    EXTERNAL_INSTALL_PROMPT = 5,
    // POST_INSTALL_PERMISSIONS_PROMPT_DEPRECATED = 6,
    // LAUNCH_PROMPT_DEPRECATED = 7,
    REMOTE_INSTALL_PROMPT = 8,
    REPAIR_PROMPT = 9,
    // DELEGATED_PERMISSIONS_PROMPT = 10,
    // DELEGATED_BUNDLE_PERMISSIONS_PROMPT_DEPRECATED = 11,
    // WEBSTORE_WIDGET_PROMPT_DEPRECATED = 12,
    EXTENSION_REQUEST_PROMPT = 13,
    EXTENSION_PENDING_REQUEST_PROMPT = 14,
    // InstallPromptData for parent to approve extension installation for
    // supervised users.
    EXTENSION_PARENT_APPROVAL_PROMPT = 15,
    NUM_PROMPT_TYPES = 16,
    // WAIT! Are you adding a new prompt type? Does it *install an extension*?
    // If not, please create a new dialog, rather than adding more functionality
    // to this class - it's already too full.
  };

  explicit InstallPromptData(PromptType type);

  InstallPromptData(const InstallPromptData&) = delete;
  InstallPromptData& operator=(const InstallPromptData&) = delete;

  ~InstallPromptData();

  void AddPermissionSet(const PermissionSet& permissions);
  void AddPermissionMessages(const PermissionMessages& permissions);
  void SetWebstoreData(const std::string& localized_user_count,
                       bool show_user_count,
                       double average_rating,
                       int rating_count,
                       const std::string& localized_rating_count);
  void SetInitialExtensionsProviderName(
      std::u16string initial_extensions_provider_name);

  PromptType type() const { return type_; }
  void set_type(PromptType type) { type_ = type; }

  // Getters for UI element labels.
  std::u16string GetDialogTitle() const;
  int GetDialogButtons() const;
  // Returns the empty string when there should be no "accept" button.
  std::u16string GetAcceptButtonLabel() const;
  std::u16string GetAbortButtonLabel() const;
  std::u16string GetPermissionsHeading() const;

  void set_requires_parent_permission(bool requires_parent_permission) {
    requires_parent_permission_ = requires_parent_permission;
  }

  bool requires_parent_permission() const {
    return requires_parent_permission_;
  }

  // Returns whether the dialog should withheld permissions if the dialog is
  // accepted.
  bool ShouldWithheldPermissionsOnDialogAccept() const;

  // Getters for webstore metadata. Only populated when the type is
  // INLINE_INSTALL_PROMPT, EXTERNAL_INSTALL_PROMPT, or REPAIR_PROMPT.
  std::tuple<int, double> GetRatingStars() const;
  std::u16string GetRatingCount() const;
  std::u16string GetUserCount() const;
  size_t GetPermissionCount() const;
  InstallPromptPermissions GetPermissions() const;
  std::u16string GetPermission(size_t index) const;

  const Extension* extension() const { return extension_.get(); }
  void set_extension(const Extension* extension) { extension_ = extension; }

  const gfx::Image& icon() const { return icon_; }
  void set_icon(const gfx::Image& icon) { icon_ = icon; }

  double average_rating() const { return average_rating_; }
  int rating_count() const { return rating_count_; }
  const std::string& localized_rating_count() const {
    return localized_rating_count_;
  }

  bool has_webstore_data() const { return has_webstore_data_; }

  void AddObserver(ExtensionInstallPromptClient::Observer* observer);
  void RemoveObserver(ExtensionInstallPromptClient::Observer* observer);

  // Called right before the dialog is about to show.
  void OnDialogOpened();

  // Called when the user clicks accept on the dialog.
  void OnDialogAccepted();

  // Called when the user clicks cancel on the dialog, presses 'x' or escape.
  void OnDialogCanceled();

 private:
  PromptType type_;

  // When this is non empty, means that this extension is an initial
  // pre-installed one.
  std::u16string initial_extensions_provider_name_;

  // Permissions that are being requested (may not be all of an extension's
  // permissions if only additional ones are being requested)
  InstallPromptPermissions prompt_permissions_;

  // True if the current user is a child.
  bool requires_parent_permission_ = false;

  bool is_requesting_host_permissions_ = false;

  // The extension being installed.
  scoped_refptr<const extensions::Extension> extension_;

  // The icon to be displayed.
  gfx::Image icon_;

  // These fields are populated only when the prompt type is
  // INLINE_INSTALL_PROMPT
  // Already formatted to be locale-specific.
  std::string localized_user_count_;
  // Range is kMinExtensionRating to kMaxExtensionRating
  double average_rating_ = 0.0;
  // The rating count for the extension, used for string pluralization.
  int rating_count_ = 0;
  // The localized rating count for the extension, used as-is for display.
  std::string localized_rating_count_;

  // Whether we should display the user count (we anticipate this will be
  // false if localized_user_count_ represents the number zero).
  bool show_user_count_ = false;

  // Whether or not this prompt has been populated with data from the
  // webstore.
  bool has_webstore_data_ = false;

  base::ObserverList<ExtensionInstallPromptClient::Observer> observers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_INSTALL_PROMPT_DATA_H_
