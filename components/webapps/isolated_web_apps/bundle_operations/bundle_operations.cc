// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/bundle_operations/bundle_operations.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/webapps/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "components/webapps/isolated_web_apps/reading/response_reader.h"
#include "components/webapps/isolated_web_apps/reading/response_reader_factory.h"
#include "components/webapps/isolated_web_apps/reading/response_reader_registry.h"
#include "components/webapps/isolated_web_apps/reading/response_reader_registry_factory.h"
#include "components/webapps/isolated_web_apps/reading/signed_web_bundle_reader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/isolated_web_apps_policy.h"

namespace web_app {

namespace {

void OnTrustAndSignaturesOfBundleChecked(
    base::OnceCallback<
        void(base::expected<web_package::SignedWebBundleIntegrityBlock,
                            std::string>)> callback,
    base::expected<std::unique_ptr<IsolatedWebAppResponseReader>,
                   UnusableSwbnFileError> result) {
  ASSIGN_OR_RETURN(
      auto reader, std::move(result), [&](const UnusableSwbnFileError& error) {
        std::move(callback).Run(base::unexpected(
            IsolatedWebAppResponseReaderFactory::ErrorToString(error)));
      });

  auto* reader_ptr = reader.get();
  base::OnceClosure reader_keep_alive =
      base::DoNothingWithBoundArgs(std::move(reader));
  reader_ptr->Close(std::move(reader_keep_alive)
                        .Then(base::BindOnce(std::move(callback),
                                             reader_ptr->GetIntegrityBlock())));
}

}  // namespace

void ReadSignedWebBundleIdInsecurely(
    const base::FilePath& path,
    base::OnceCallback<void(base::expected<web_package::SignedWebBundleId,
                                           std::string>)> callback) {
  UnsecureSignedWebBundleIdReader::GetWebBundleId(
      path,
      base::BindOnce([](base::expected<web_package::SignedWebBundleId,
                                       UnusableSwbnFileError> result) {
        return result.transform_error([](const UnusableSwbnFileError& error) {
          return "Failed to read the integrity block of the "
                 "signed web bundle: " +
                 error.message();
        });
      }).Then(std::move(callback)));
}

void ValidateSignedWebBundleTrustAndSignatures(
    content::BrowserContext* browser_context,
    const base::FilePath& path,
    const web_package::SignedWebBundleId& expected_web_bundle_id,
    bool is_dev_mode_bundle,
    base::OnceCallback<
        void(base::expected<web_package::SignedWebBundleIntegrityBlock,
                            std::string>)> callback) {
  auto reader_factory =
      std::make_unique<IsolatedWebAppResponseReaderFactory>(browser_context);
  IsolatedWebAppResponseReaderFactory::Flags flags;
  if (is_dev_mode_bundle) {
    flags.Put(IsolatedWebAppResponseReaderFactory::Flag::kDevModeBundle);
  }
  auto* reader_factory_ptr = reader_factory.get();
  reader_factory_ptr->CreateResponseReader(
      path, expected_web_bundle_id, flags,
      base::BindOnce(&OnTrustAndSignaturesOfBundleChecked, std::move(callback))
          .Then(base::OnceClosure(
              base::DoNothingWithBoundArgs(std::move(reader_factory)))));
}

void CloseBundle(content::BrowserContext* browser_context,
                 const base::FilePath& path,
                 base::OnceClosure callback) {
  IsolatedWebAppReaderRegistryFactory::Get(browser_context)
      ->ClearCacheForPath(path, std::move(callback));
}

}  // namespace web_app
