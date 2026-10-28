// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_invariants.h"

#include <optional>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/mime_util.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace blink {

std::string_view ManifestInvariantErrorToString(ManifestInvariantError error) {
  switch (error) {
    case ManifestInvariantError::kStartUrlInvalid:
      return "Manifest start_url is invalid.";
    case ManifestInvariantError::kIdInvalid:
      return "Manifest id is invalid.";
    case ManifestInvariantError::kScopeInvalid:
      return "Manifest scope is invalid.";
    case ManifestInvariantError::kOriginOpaque:
      return "Manifest start_url origin must not be opaque.";
    case ManifestInvariantError::kIdCrossOrigin:
      return "Manifest id must be same-origin with the document.";
    case ManifestInvariantError::kScopeCrossOrigin:
      return "Manifest scope must be same-origin with the document.";
    case ManifestInvariantError::kShareTargetActionCrossOrigin:
      return "Manifest share_target must be same-origin with the document.";
    case ManifestInvariantError::kFileHandlerActionCrossOrigin:
      return "Manifest file_handlers must be same-origin with the document.";
    case ManifestInvariantError::kFileHandlerInvalidMimeType:
      return "Manifest file_handlers accept MIME type is invalid.";
    case ManifestInvariantError::kFileHandlerEmptyExtensionList:
      return "Manifest file_handlers accept extensions list cannot be empty.";
    case ManifestInvariantError::kFileHandlerInvalidExtension:
      return "Manifest file_handlers accept extension must start with a '.' "
             "and contain at least one extension character.";
    case ManifestInvariantError::kFileHandlerExtensionContainsControlOrFormat:
      return "Manifest file_handlers accept extension contains invalid "
             "control or format characters.";
    case ManifestInvariantError::kProtocolHandlerUrlCrossOrigin:
      return "Manifest protocol_handlers must be same-origin with the "
             "document.";
    case ManifestInvariantError::kProtocolHandlerInvalidScheme:
      return "Manifest protocol_handlers protocol is invalid or restricted for "
             "security level.";
    case ManifestInvariantError::kNoteTakingUrlCrossOrigin:
      return "Manifest note_taking new_note_url must be same-origin with the "
             "document.";
    case ManifestInvariantError::kLockScreenStartUrlCrossOrigin:
      return "Manifest lock_screen start_url must be same-origin with the "
             "document.";
    case ManifestInvariantError::kMigrateFromIdInvalid:
      return "Manifest migrate_from id must be valid.";
    case ManifestInvariantError::kMigrateFromIdCrossSite:
      return "Manifest migrate_from id must be the same site as the document.";
    case ManifestInvariantError::kMigrateFromInstallUrlInvalid:
      return "Manifest migrate_from install_url must be valid.";
    case ManifestInvariantError::kMigrateFromInstallUrlCrossSite:
      return "Manifest migrate_from install_url must be the same site as the "
             "document.";
    case ManifestInvariantError::kMigrateFromInstallUrlCrossOriginWithId:
      return "Manifest migrate_from install_url must be the same origin as the "
             "id.";
    case ManifestInvariantError::kMigrateToIdInvalid:
      return "Manifest migrate_to id must be valid.";
    case ManifestInvariantError::kMigrateToIdCrossSite:
      return "Manifest migrate_to id must be the same site as the document.";
    case ManifestInvariantError::kMigrateToInstallUrlInvalid:
      return "Manifest migrate_to install_url must be valid.";
    case ManifestInvariantError::kMigrateToInstallUrlCrossSite:
      return "Manifest migrate_to install_url must be the same site as the "
             "document.";
    case ManifestInvariantError::kMigrateToInstallUrlCrossOriginWithId:
      return "Manifest migrate_to install_url must be the same origin as the "
             "id.";
    case ManifestInvariantError::kShortcutUrlInvalid:
      return "Manifest shortcut urls must be valid.";
    case ManifestInvariantError::kShortcutUrlOutOfScope:
      return "Manifest shortcut urls must be within scope.";
    case ManifestInvariantError::kScopeExtensionOriginOpaque:
      return "Manifest scope_extensions origin must not be opaque.";
    case ManifestInvariantError::kScopeExtensionSchemeNotHttps:
      return "Manifest scope_extensions origin must use the https scheme.";
    case ManifestInvariantError::kIconSrcInvalid:
      return "Manifest icon urls must be valid.";
    case ManifestInvariantError::kIconSrcInvalidScheme:
      return "Manifest icon urls must be http, https, data, or match the "
             "document scheme.";
  }
  NOTREACHED();
}

std::optional<ManifestInvariantError> Manifest::ValidateInvariants() const {
  // `start_url`, `id`, and `scope` must be valid.
  if (!start_url_.is_valid()) {
    return ManifestInvariantError::kStartUrlInvalid;
  }
  if (!id_.is_valid()) {
    return ManifestInvariantError::kIdInvalid;
  }
  if (!scope_.is_valid()) {
    return ManifestInvariantError::kScopeInvalid;
  }

  // Origin of `start_url` must be non-opaque.
  url::Origin origin = url::Origin::Create(start_url_);
  if (origin.opaque()) {
    return ManifestInvariantError::kOriginOpaque;
  }

  // `id` and `scope` must be same-origin with `start_url`.
  if (!origin.IsSameOriginWith(id_)) {
    return ManifestInvariantError::kIdCrossOrigin;
  }
  if (!origin.IsSameOriginWith(scope_)) {
    return ManifestInvariantError::kScopeCrossOrigin;
  }

  // `share_target.action` must be same-origin with `start_url`.
  if (share_target_ && !origin.IsSameOriginWith(share_target_->action)) {
    return ManifestInvariantError::kShareTargetActionCrossOrigin;
  }

  // File handlers validation.
  for (const auto& file_handler : file_handlers_) {
    if (!origin.IsSameOriginWith(file_handler.action)) {
      return ManifestInvariantError::kFileHandlerActionCrossOrigin;
    }
    for (const auto& [mime_type, extensions] : file_handler.accept) {
      std::string mime_type_utf8 = base::UTF16ToUTF8(mime_type);
      std::string top_level_mime_type;
      if (!net::ParseMimeTypeWithoutParameter(mime_type_utf8,
                                              &top_level_mime_type,
                                              /*subtype=*/nullptr) ||
          !net::IsValidTopLevelMimeType(top_level_mime_type)) {
        return ManifestInvariantError::kFileHandlerInvalidMimeType;
      }
      if (extensions.empty()) {
        return ManifestInvariantError::kFileHandlerEmptyExtensionList;
      }
      for (const auto& extension : extensions) {
        if (!extension.starts_with(u".") || extension.length() <= 1) {
          return ManifestInvariantError::kFileHandlerInvalidExtension;
        }
        for (size_t i = 0; i < extension.length();) {
          UChar32 c;
          U16_NEXT(extension, i, extension.length(), c);
          if (base::IsUnicodeControl(c) || u_charType(c) == U_FORMAT_CHAR) {
            return ManifestInvariantError::
                kFileHandlerExtensionContainsControlOrFormat;
          }
        }
      }
    }
  }

  // Protocol handlers validation.
  for (const auto& protocol_handler : protocol_handlers_) {
    if (!origin.IsSameOriginWith(protocol_handler.url)) {
      return ManifestInvariantError::kProtocolHandlerUrlCrossOrigin;
    }
    blink::ProtocolHandlerSecurityLevel security_level =
        blink::CommonSchemeRegistry::IsIsolatedAppScheme(origin.scheme())
            ? blink::ProtocolHandlerSecurityLevel::kIsolatedAppFeatures
            : blink::ProtocolHandlerSecurityLevel::kStrict;
    if (!blink::IsValidCustomHandlerScheme(
            base::UTF16ToUTF8(protocol_handler.protocol), security_level)) {
      return ManifestInvariantError::kProtocolHandlerInvalidScheme;
    }
  }

  // Note taking validation (if new_note_url is valid, must be same-origin).
  if (note_taking_ && note_taking_->new_note_url.is_valid() &&
      !origin.IsSameOriginWith(note_taking_->new_note_url)) {
    return ManifestInvariantError::kNoteTakingUrlCrossOrigin;
  }

  // Lock screen validation (if start_url is valid, must be same-origin).
  if (lock_screen_ && lock_screen_->start_url.is_valid() &&
      !origin.IsSameOriginWith(lock_screen_->start_url)) {
    return ManifestInvariantError::kLockScreenStartUrlCrossOrigin;
  }

  // migrate-from validation.
  net::SchemefulSite site(origin);
  for (const auto& migrate_from_item : migrate_from_) {
    if (!migrate_from_item.id.is_valid()) {
      return ManifestInvariantError::kMigrateFromIdInvalid;
    }
    if (!site.IsSameSiteWith(migrate_from_item.id)) {
      return ManifestInvariantError::kMigrateFromIdCrossSite;
    }
    if (migrate_from_item.install_url) {
      if (!migrate_from_item.install_url->is_valid()) {
        return ManifestInvariantError::kMigrateFromInstallUrlInvalid;
      }
      if (!site.IsSameSiteWith(*migrate_from_item.install_url)) {
        return ManifestInvariantError::kMigrateFromInstallUrlCrossSite;
      }
      if (!url::IsSameOriginWith(migrate_from_item.id,
                                 *migrate_from_item.install_url)) {
        return ManifestInvariantError::kMigrateFromInstallUrlCrossOriginWithId;
      }
    }
  }

  // migrate-to validation.
  if (migrate_to_) {
    if (!migrate_to_->id.is_valid()) {
      return ManifestInvariantError::kMigrateToIdInvalid;
    }
    if (!site.IsSameSiteWith(migrate_to_->id)) {
      return ManifestInvariantError::kMigrateToIdCrossSite;
    }
    if (!migrate_to_->install_url.is_valid()) {
      return ManifestInvariantError::kMigrateToInstallUrlInvalid;
    }
    if (!site.IsSameSiteWith(migrate_to_->install_url)) {
      return ManifestInvariantError::kMigrateToInstallUrlCrossSite;
    }
    if (!url::IsSameOriginWith(migrate_to_->id, migrate_to_->install_url)) {
      return ManifestInvariantError::kMigrateToInstallUrlCrossOriginWithId;
    }
  }

  // Shortcuts validation.
  for (const auto& shortcut : shortcuts_) {
    if (!shortcut.url.is_valid()) {
      return ManifestInvariantError::kShortcutUrlInvalid;
    }
    if (!(url::IsSameOriginWith(scope_, shortcut.url) &&
          base::StartsWith(shortcut.url.path(), scope_.path(),
                           base::CompareCase::SENSITIVE))) {
      return ManifestInvariantError::kShortcutUrlOutOfScope;
    }
  }

  // Scope extensions validation.
  for (const auto& scope_extension : scope_extensions_) {
    if (scope_extension.origin.opaque()) {
      return ManifestInvariantError::kScopeExtensionOriginOpaque;
    }
    if (scope_extension.origin.scheme() != url::kHttpsScheme) {
      return ManifestInvariantError::kScopeExtensionSchemeNotHttps;
    }
  }

  // Icons validation.
  for (const auto& icon : icons_) {
    if (!icon.src.is_valid()) {
      return ManifestInvariantError::kIconSrcInvalid;
    }
    if (!icon.src.SchemeIsHTTPOrHTTPS() && !icon.src.SchemeIs("data") &&
        !icon.src.SchemeIs(origin.scheme())) {
      return ManifestInvariantError::kIconSrcInvalidScheme;
    }
  }

  return std::nullopt;
}

}  // namespace blink
