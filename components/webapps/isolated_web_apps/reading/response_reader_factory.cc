// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/reading/response_reader_factory.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/webapps/isolated_web_apps/client.h"
#include "components/webapps/isolated_web_apps/error/uma_logging.h"
#include "components/webapps/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"
#include "components/webapps/isolated_web_apps/reading/signed_web_bundle_reader.h"
#include "components/webapps/isolated_web_apps/reading/validator.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"

namespace web_app {

IsolatedWebAppResponseReaderFactory::IsolatedWebAppResponseReaderFactory(
    content::BrowserContext* browser_context)
    : browser_context_(*browser_context) {}

IsolatedWebAppResponseReaderFactory::~IsolatedWebAppResponseReaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IsolatedWebAppResponseReaderFactory::CreateResponseReader(
    const base::FilePath& web_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    bool verify_signatures,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!web_bundle_id.is_for_proxy_mode());

  auto create_reader = base::BindOnce(
      &SignedWebBundleReader::Create, web_bundle_path,
      IwaOrigin(web_bundle_id).origin().GetURL(), verify_signatures,
      base::BindOnce(&IsolatedWebAppResponseReaderFactory::OnReaderCreated,
                     weak_ptr_factory_.GetWeakPtr(), web_bundle_path,
                     web_bundle_id, std::move(callback)));

  if (auto* provider = IwaClient::GetInstance()->GetRuntimeDataProvider()) {
    provider->OnBestEffortRuntimeDataReady().Post(FROM_HERE,
                                                  std::move(create_reader));
  } else {
    std::move(create_reader).Run();
  }
}

void IsolatedWebAppResponseReaderFactory::OnReaderCreated(
    const base::FilePath& web_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    Callback callback,
    base::expected<std::unique_ptr<SignedWebBundleReader>,
                   UnusableSwbnFileError> status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ASSIGN_OR_RETURN(auto reader, std::move(status), [&](const auto& error) {
    UmaLogExpectedStatus<UnusableSwbnFileError>(
        "WebApp.Isolated.SwbnFileUsability", base::unexpected(error));
    std::move(callback).Run(base::unexpected(error));
  });

  RETURN_IF_ERROR(
      IsolatedWebAppValidator::ValidateIntegrityBlockAndMetadata(
          &browser_context_.get(), web_bundle_id, reader->GetIntegrityBlock(),
          reader->GetPrimaryURL(), reader->GetEntries()),
      [&](const auto& error) {
        UmaLogExpectedStatus<UnusableSwbnFileError>(
            "WebApp.Isolated.SwbnFileUsability", base::unexpected(error));
        // Since `reader` is initialized at this point, it's also necessary to
        // properly dispose of it before invoking `callback`.
        auto close_reader = base::BindOnce(
            [](std::unique_ptr<SignedWebBundleReader> reader,
               base::OnceClosure callback) {
              auto* reader_ptr = reader.get();
              base::OnceClosure reader_keepalive =
                  base::DoNothingWithBoundArgs(std::move(reader));
              reader_ptr->Close(
                  std::move(callback).Then(std::move(reader_keepalive)));
            },
            std::move(reader));
        std::move(close_reader)
            .Run(base::BindOnce(std::move(callback), base::unexpected(error)));
      });

  UmaLogExpectedStatus<UnusableSwbnFileError>(
      "WebApp.Isolated.SwbnFileUsability", base::ok());

  std::move(callback).Run(std::make_unique<IsolatedWebAppResponseReaderImpl>(
      std::move(reader), &browser_context_.get()));
}

}  // namespace web_app
