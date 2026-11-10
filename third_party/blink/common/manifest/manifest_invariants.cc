// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_invariants.h"

#include <optional>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
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
  if (!start_url().is_valid()) {
    return ManifestInvariantError::kStartUrlInvalid;
  }
  if (!id().is_valid()) {
    return ManifestInvariantError::kIdInvalid;
  }
  if (!scope().is_valid()) {
    return ManifestInvariantError::kScopeInvalid;
  }

  // Origin of `start_url` must be non-opaque.
  const url::Origin& origin = GetOrigin();
  if (origin.opaque()) {
    return ManifestInvariantError::kOriginOpaque;
  }

  // `id` and `scope` must be same-origin with `start_url`.
  if (!origin.IsSameOriginWith(id())) {
    return ManifestInvariantError::kIdCrossOrigin;
  }
  if (!origin.IsSameOriginWith(scope())) {
    return ManifestInvariantError::kScopeCrossOrigin;
  }

  // `share_target.action` must be same-origin with `start_url`.
  if (share_target() && !origin.IsSameOriginWith(share_target()->action)) {
    return ManifestInvariantError::kShareTargetActionCrossOrigin;
  }

  // File handlers validation.
  for (const auto& file_handler : file_handlers()) {
    if (!origin.IsSameOriginWith(file_handler.action)) {
      return ManifestInvariantError::kFileHandlerActionCrossOrigin;
    }
    for (const auto& [mime_type, extensions] : file_handler.accept) {
      if (!IsValidFileHandlerMimeType(base::UTF16ToUTF8(mime_type))) {
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
  if (!protocol_handlers().empty()) {
    const bool is_isolated_app =
        blink::CommonSchemeRegistry::IsIsolatedAppScheme(origin.scheme());
    const blink::ProtocolHandlerSecurityLevel security_level =
        is_isolated_app
            ? blink::ProtocolHandlerSecurityLevel::kIsolatedAppFeatures
            : blink::ProtocolHandlerSecurityLevel::kStrict;
    for (const auto& protocol_handler : protocol_handlers()) {
      if (!origin.IsSameOriginWith(protocol_handler.url)) {
        return ManifestInvariantError::kProtocolHandlerUrlCrossOrigin;
      }
      if (!blink::IsValidCustomHandlerScheme(
              base::UTF16ToUTF8(protocol_handler.protocol), security_level)) {
        return ManifestInvariantError::kProtocolHandlerInvalidScheme;
      }
    }
  }

  // Note taking validation (if new_note_url is valid, must be same-origin).
  if (note_taking() && note_taking()->new_note_url.is_valid() &&
      !origin.IsSameOriginWith(note_taking()->new_note_url)) {
    return ManifestInvariantError::kNoteTakingUrlCrossOrigin;
  }

  // Lock screen validation (if start_url is valid, must be same-origin).
  if (lock_screen() && lock_screen()->start_url.is_valid() &&
      !origin.IsSameOriginWith(lock_screen()->start_url)) {
    return ManifestInvariantError::kLockScreenStartUrlCrossOrigin;
  }

  // migrate-from and migrate-to validation.
  if (!migrate_from().empty() || migrate_to().has_value()) {
    net::SchemefulSite site(origin);
    for (const auto& migrate_from_item : migrate_from()) {
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
          return ManifestInvariantError::
              kMigrateFromInstallUrlCrossOriginWithId;
        }
      }
    }

    if (migrate_to()) {
      if (!migrate_to()->id.is_valid()) {
        return ManifestInvariantError::kMigrateToIdInvalid;
      }
      if (!site.IsSameSiteWith(migrate_to()->id)) {
        return ManifestInvariantError::kMigrateToIdCrossSite;
      }
      if (!migrate_to()->install_url.is_valid()) {
        return ManifestInvariantError::kMigrateToInstallUrlInvalid;
      }
      if (!site.IsSameSiteWith(migrate_to()->install_url)) {
        return ManifestInvariantError::kMigrateToInstallUrlCrossSite;
      }
      if (!url::IsSameOriginWith(migrate_to()->id, migrate_to()->install_url)) {
        return ManifestInvariantError::kMigrateToInstallUrlCrossOriginWithId;
      }
    }
  }

  // Shortcuts validation.
  if (!shortcuts().empty()) {
    const std::string_view scope_path = scope().path();
    for (const auto& shortcut : shortcuts()) {
      if (!shortcut.url.is_valid()) {
        return ManifestInvariantError::kShortcutUrlInvalid;
      }
      if (!(origin.IsSameOriginWith(shortcut.url) &&
            base::StartsWith(shortcut.url.path(), scope_path,
                             base::CompareCase::SENSITIVE))) {
        return ManifestInvariantError::kShortcutUrlOutOfScope;
      }
    }
  }

  // Scope extensions validation.
  for (const auto& scope_extension : scope_extensions()) {
    if (scope_extension.origin.opaque()) {
      return ManifestInvariantError::kScopeExtensionOriginOpaque;
    }
    if (scope_extension.origin.scheme() != url::kHttpsScheme) {
      return ManifestInvariantError::kScopeExtensionSchemeNotHttps;
    }
  }

  // Icons validation.
  for (const auto& icon : icons()) {
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
