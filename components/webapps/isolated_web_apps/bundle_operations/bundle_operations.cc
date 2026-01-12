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
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/webapps/isolated_web_apps/client.h"
#include "components/webapps/isolated_web_apps/error/uma_logging.h"
#include "components/webapps/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "components/webapps/isolated_web_apps/reading/response_reader_registry.h"
#include "components/webapps/isolated_web_apps/reading/response_reader_registry_factory.h"
#include "components/webapps/isolated_web_apps/reading/signed_web_bundle_reader.h"
#include "components/webapps/isolated_web_apps/reading/validator.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/isolated_web_apps_policy.h"

namespace web_app {

namespace {

using IntegrityBlockResult =
    base::expected<web_package::SignedWebBundleIntegrityBlock, std::string>;

void OnTrustAndSignaturesOfBundleChecked(
    base::WeakPtr<content::BrowserContext> browser_context,
    const web_package::SignedWebBundleId& expected_web_bundle_id,
    base::OnceCallback<void(IntegrityBlockResult)> callback,
    base::expected<std::unique_ptr<SignedWebBundleReader>,
                   UnusableSwbnFileError> status) {
  if (!browser_context) {
    std::move(callback).Run(base::unexpected("Invalid browser_context."));
    return;
  }

  ASSIGN_OR_RETURN(
      auto reader, std::move(status), [&](const UnusableSwbnFileError& error) {
        UmaLogExpectedStatus<UnusableSwbnFileError>(
            "WebApp.Isolated.SwbnFileUsability", base::unexpected(error));
        std::move(callback).Run(base::unexpected(error.ToString()));
      });

  auto validation_result =
      IsolatedWebAppValidator::ValidateIntegrityBlockAndMetadata(
          browser_context.get(), expected_web_bundle_id,
          reader->GetIntegrityBlock(), reader->GetPrimaryURL(),
          reader->GetEntries());
  UmaLogExpectedStatus("WebApp.Isolated.SwbnFileUsability", validation_result);

  IntegrityBlockResult integrity_block_result =
      validation_result.transform([&] { return reader->GetIntegrityBlock(); })
          .transform_error(&UnusableSwbnFileError::ToString);

  auto* reader_ptr = reader.get();
  reader_ptr->Close(
      base::OnceClosure(base::DoNothingWithBoundArgs(std::move(reader)))
          .Then(base::BindOnce(std::move(callback),
                               std::move(integrity_block_result))));
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

void ValidateSignedWebBundleSignatures(
    content::BrowserContext* browser_context,
    const base::FilePath& path,
    const web_package::SignedWebBundleId& expected_web_bundle_id,
    base::OnceCallback<void(IntegrityBlockResult)> callback) {
  auto create_reader = base::BindOnce(
      &SignedWebBundleReader::Create, path,
      IwaOrigin(expected_web_bundle_id).origin().GetURL(),
      /*verify_signatures=*/true,
      base::BindOnce(&OnTrustAndSignaturesOfBundleChecked,
                     browser_context->GetWeakPtr(), expected_web_bundle_id,
                     std::move(callback)));

  if (auto* provider = IwaClient::GetInstance()->GetRuntimeDataProvider()) {
    provider->OnBestEffortRuntimeDataReady().Post(FROM_HERE,
                                                  std::move(create_reader));
  } else {
    std::move(create_reader).Run();
  }
}

void CloseBundle(content::BrowserContext* browser_context,
                 const base::FilePath& path,
                 base::OnceClosure callback) {
  IsolatedWebAppReaderRegistryFactory::Get(browser_context)
      ->ClearCacheForPath(path, std::move(callback));
}

}  // namespace web_app
