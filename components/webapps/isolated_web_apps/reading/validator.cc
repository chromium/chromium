// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/reading/validator.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/web_package/signed_web_bundles/identity_validator.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/webapps/isolated_web_apps/client.h"
#include "components/webapps/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace web_app {

namespace {

base::expected<void, std::string> ValidateIntegrityBlockImpl(
    content::BrowserContext* browser_context,
    const web_package::SignedWebBundleId& expected_web_bundle_id,
    const web_package::SignedWebBundleIntegrityBlock& integrity_block,
    bool dev_mode) {
  if (expected_web_bundle_id.is_for_proxy_mode()) {
    return base::unexpected(
        "Web Bundle IDs of type ProxyMode are not supported.");
  }

  auto derived_web_bundle_id = integrity_block.web_bundle_id();
  if (derived_web_bundle_id != expected_web_bundle_id) {
    return base::unexpected(base::StringPrintf(
        "The Web Bundle ID (%s) derived from the integrity block does not "
        "match the expected Web Bundle ID (%s).",
        derived_web_bundle_id.id().c_str(),
        expected_web_bundle_id.id().c_str()));
  }

  RETURN_IF_ERROR(
      web_package::IdentityValidator::GetInstance()->ValidateWebBundleIdentity(
          derived_web_bundle_id.id(),
          integrity_block.signature_stack().public_keys()));

  RETURN_IF_ERROR(IwaClient::GetInstance()->ValidateTrust(
      browser_context, expected_web_bundle_id, dev_mode));

  return base::ok();
}

base::expected<void, std::string> ValidateMetadataImpl(
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::optional<GURL>& primary_url,
    const std::vector<GURL>& entries) {
  // Verify that the Signed Web Bundle does not have a primary URL set.
  // Primary URLs make no sense for Isolated Web Apps - the "primary URL"
  // should be retrieved from the web app manifest's `start_url` field.
  if (primary_url.has_value()) {
    return base::unexpected("Primary URL must not be present, but was " +
                            primary_url->possibly_invalid_spec());
  }

  // Verify that the bundle only contains isolated-app:// URLs using the
  // Signed Web Bundle ID as their host.
  for (const GURL& entry : entries) {
    ASSIGN_OR_RETURN(web_package::SignedWebBundleId entry_web_bundle_id,
                     IwaOrigin::Create(entry)
                         .transform([](const auto& iwa_origin) {
                           return iwa_origin.web_bundle_id();
                         })
                         .transform_error([](std::string error) {
                           return "The URL of an exchange is invalid: " +
                                  std::move(error);
                         }));
    if (entry_web_bundle_id != web_bundle_id) {
      return base::unexpected(
          "The URL of an exchange contains the wrong Signed Web Bundle ID: " +
          entry_web_bundle_id.id());
    }
    if (entry.has_ref()) {
      return base::unexpected(
          "The URL of an exchange is invalid: URLs must not have a fragment "
          "part.");
    }
    if (entry.has_query()) {
      return base::unexpected(
          "The URL of an exchange is invalid: URLs must not have a query "
          "part.");
    }
  }

  return base::ok();
}

}  // namespace

// static
base::expected<void, UnusableSwbnFileError>
IsolatedWebAppValidator::ValidateIntegrityBlock(
    content::BrowserContext* browser_context,
    const web_package::SignedWebBundleId& expected_web_bundle_id,
    const web_package::SignedWebBundleIntegrityBlock& integrity_block,
    bool dev_mode) {
  return ValidateIntegrityBlockImpl(browser_context, expected_web_bundle_id,
                                    integrity_block, dev_mode)
      .transform_error([](const auto& error) {
        return UnusableSwbnFileError(
            UnusableSwbnFileError::Error::kIntegrityBlockValidationError,
            error);
      });
}

// static
base::expected<void, UnusableSwbnFileError>
IsolatedWebAppValidator::ValidateMetadata(
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::optional<GURL>& primary_url,
    const std::vector<GURL>& entries) {
  return ValidateMetadataImpl(web_bundle_id, primary_url, entries)
      .transform_error([](const auto& error) {
        return UnusableSwbnFileError(
            UnusableSwbnFileError::Error::kMetadataValidationError, error);
      });
}

}  // namespace web_app
