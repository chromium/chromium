// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TEST_SUPPORT_SIGNING_KEYS_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TEST_SUPPORT_SIGNING_KEYS_H_

#include "components/web_package/test_support/signed_web_bundles/key_pair.h"
#include "components/web_package/test_support/signed_web_bundles/signing_keys.h"

namespace web_app::test {

using web_package::test::KeyPair;
using web_package::test::KeyPairs;

// Pieces related to Ed25519 keys:
using web_package::test::GetDefaultEd25519KeyPair;
using web_package::test::GetDefaultEd25519WebBundleId;

// Pieces related to EcdsaP256 keys:
using web_package::test::GetDefaultEcdsaP256KeyPair;
using web_package::test::GetDefaultEcdsaP256WebBundleId;

}  // namespace web_app::test

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TEST_SUPPORT_SIGNING_KEYS_H_
