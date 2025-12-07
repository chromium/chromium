// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_BUNDLE_OPERATIONS_BUNDLE_OPERATIONS_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_BUNDLE_OPERATIONS_BUNDLE_OPERATIONS_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace web_app {

// Asynchronously reads the Web Bundle ID from the integrity block of the
// bundle at `path`. This is an insecure operation as it does NOT verify the
// bundle's signatures. It should only be used to discover the ID of a bundle
// from an untrusted source before it can be validated.
void ReadSignedWebBundleIdInsecurely(
    const base::FilePath& path,
    base::OnceCallback<void(
        base::expected<web_package::SignedWebBundleId, std::string>)> callback);

// Asynchronously validates the integrity and trust of a Signed Web Bundle.
//
// This function performs a full security check, including:
//  - Verifying that the bundle's ID matches the `expected_web_bundle_id`.
//  - Verifying the bundle's cryptographic signatures.
//  - Validating that the bundle is trusted for the given `browser_context`
//    by checking enterprise policies, dev mode status, etc.
//
// On success, the callback is run with the parsed integrity block.
// On failure, the callback is run with a descriptive error message.
void ValidateSignedWebBundleTrustAndSignatures(
    content::BrowserContext* browser_context,
    const base::FilePath& path,
    const web_package::SignedWebBundleId& expected_web_bundle_id,
    bool is_dev_mode_bundle,
    base::OnceCallback<
        void(base::expected<web_package::SignedWebBundleIntegrityBlock,
                            std::string>)> callback);

// Asynchronously closes the Signed Web Bundle at the given `path`, releasing
// any resources (like open file handles) that the browser may be holding for
// it. The callback is run once the bundle has been closed. This should be
// called before attempting to delete the bundle file from disk.
void CloseBundle(content::BrowserContext* browser_context,
                 const base::FilePath& path,
                 base::OnceClosure callback);

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_BUNDLE_OPERATIONS_BUNDLE_OPERATIONS_H_
