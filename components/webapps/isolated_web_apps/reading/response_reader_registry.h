// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_READING_RESPONSE_READER_REGISTRY_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_READING_RESPONSE_READER_REGISTRY_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"
#include "components/webapps/isolated_web_apps/reading/response_reader.h"
#include "components/webapps/isolated_web_apps/reading/response_reader_factory.h"
#include "services/network/public/cpp/resource_request.h"

namespace web_package {
class SignedWebBundleId;
}  // namespace web_package

namespace web_app {

// A registry to create and keep track of `IsolatedWebAppResponseReader`
// instances used to read Isolated Web Apps. At its core, it contains a map from
// file paths to `IsolatedWebAppResponseReader`s to cache them for repeated
// calls. On non-ChromeOS devices, the first request for a particular file path
// will also check the integrity of the Signed Web Bundle. On ChromeOS, it is
// assumed that the Signed Web Bundle has not been corrupted due to its location
// inside cryptohome, and signatures are not checked.
class IsolatedWebAppReaderRegistry : public KeyedService,
                                     public IwaRuntimeDataProvider::Observer {
 public:
  IsolatedWebAppReaderRegistry(
      content::BrowserContext* browser_context,
      std::unique_ptr<IsolatedWebAppResponseReaderFactory> reader_factory);
  ~IsolatedWebAppReaderRegistry() override;

  IsolatedWebAppReaderRegistry(const IsolatedWebAppReaderRegistry&) = delete;
  IsolatedWebAppReaderRegistry& operator=(const IsolatedWebAppReaderRegistry&) =
      delete;

  struct ReadResponseError {
    enum class Type {
      kOtherError,
      kResponseNotFound,
    };

    static ReadResponseError ForError(const UnusableSwbnFileError& error);

    static ReadResponseError ForError(
        const IsolatedWebAppResponseReader::Error& error);

    static ReadResponseError ForOtherError(const std::string& message) {
      return ReadResponseError(Type::kOtherError, message);
    }

    Type type;
    std::string message;

   private:
    static ReadResponseError ForResponseNotFound(const std::string& message) {
      return ReadResponseError(Type::kResponseNotFound, message);
    }

    ReadResponseError(Type type, const std::string& message)
        : type(type), message(message) {}
  };

  using ReadResponseCallback = base::OnceCallback<void(
      base::expected<IsolatedWebAppResponseReader::Response, ReadResponseError>
          response)>;

  // Given a path to a Signed Web Bundle, the expected Signed Web Bundle ID, and
  // a request, read the corresponding response from it. The `callback` receives
  // both the response head and a closure it can call to read the response body,
  // or a string if an error occurs.
  void ReadResponse(const base::FilePath& web_bundle_path,
                    bool dev_mode,
                    const web_package::SignedWebBundleId& web_bundle_id,
                    const network::ResourceRequest& resource_request,
                    ReadResponseCallback callback);

  // Closes the cached readers of the given path. After callback is invoked the
  // caller can expect that the corresponding file is closed.
  void ClearCacheForPath(const base::FilePath& web_bundle_path,
                         base::OnceClosure callback);

  // This enum represents every error type that can occur during response head
  // parsing, after integrity block and metadata have been read successfully.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ReadResponseHeadError {
    kResponseHeadParserInternalError = 1,
    kResponseHeadParserFormatError = 2,
    kResponseNotFoundError = 3,
    kAppNotTrusted = 4,
    kMaxValue = kAppNotTrusted
  };

 private:
  class Cache;

  FRIEND_TEST_ALL_PREFIXES(IsolatedWebAppReaderRegistryTest,
                           TestConcurrentRequests);
  FRIEND_TEST_ALL_PREFIXES(IsolatedWebAppReaderRegistryTest,
                           TestSignedWebBundleReaderLifetime);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ReaderCacheState {
    kNotCached = 0,
    kCachedReady = 1,
    kCachedPending = 2,
    kMaxValue = kCachedPending
  };

  bool IsCleanupTimerRunningForTesting() const;

  // IwaRuntimeDataProvider::Observer:
  void OnRuntimeDataChanged() override;

  void OnResponseReaderCreated(
      const base::FilePath& web_bundle_path,
      const web_package::SignedWebBundleId& web_bundle_id,
      base::expected<std::unique_ptr<IsolatedWebAppResponseReader>,
                     UnusableSwbnFileError> reader);

  void DoReadResponse(IsolatedWebAppResponseReader& reader,
                      network::ResourceRequest resource_request,
                      ReadResponseCallback callback);

  void OnResponseRead(
      ReadResponseCallback callback,
      base::expected<IsolatedWebAppResponseReader::Response,
                     IsolatedWebAppResponseReader::Error> response);

  base::ScopedObservation<IwaRuntimeDataProvider,
                          IwaRuntimeDataProvider::Observer>
      key_provider_observation_{this};

  // A set of files whose signatures have been verified successfully during the
  // current browser session. Signatures of these files are not re-verified even
  // if their corresponding `CacheEntry` is cleaned up and later re-created.
  base::flat_set<base::FilePath> verified_files_;

  const raw_ref<content::BrowserContext> browser_context_;

  std::unique_ptr<IsolatedWebAppResponseReaderFactory> reader_factory_;

  std::unique_ptr<Cache> cache_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<IsolatedWebAppReaderRegistry> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_READING_RESPONSE_READER_REGISTRY_H_
