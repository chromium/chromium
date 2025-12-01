// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/reading/response_reader_registry.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <variant>

#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/web_package/signed_web_bundles/identity_validator.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/client.h"
#include "components/webapps/isolated_web_apps/error/uma_logging.h"
#include "components/webapps/isolated_web_apps/reading/response_reader.h"
#include "components/webapps/isolated_web_apps/reading/response_reader_factory.h"
#include "components/webapps/isolated_web_apps/reading/signed_web_bundle_reader.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace web_app {

namespace {

constexpr std::string_view kReaderCacheStateHistogramName =
    "WebApp.Isolated.ResponseReaderCacheState";

// References to `SignedWebBundleReader`s that are not used for the provided
// time interval will be removed from the cache. This is important so that the
// cache doesn't grow forever, given that each `SignedWebBundleReader` requires
// some memory and an open file handle.
//
// Note: Depending on when during the interval a new `SignedWebBundleReader` is
// accessed, the worst-case time until it is cleaned up can be up to two times
// `kCleanupInterval`, since the logic for cleaning up `SignedWebBundleReader`s
// is as follows: Every `kCleanupInterval`, remove references to all
// `SignedWebBundleReader`s that haven't been accessed for at least
// `kCleanupInterval`.
// We could run a separate timer per `SignedWebBundleReader` to more accurately
// respect `kCleanupInterval`, but this feels like unnecessary overhead.
base::TimeDelta kCleanupInterval = base::Minutes(10);

base::expected<void, IsolatedWebAppReaderRegistry::ReadResponseHeadError>
ToReadResponseHeadError(
    const base::expected<IsolatedWebAppResponseReader::Response,
                         IsolatedWebAppResponseReader::Error>& response) {
  if (response.has_value()) {
    return base::ok();
  }
  switch (response.error().type) {
    case IsolatedWebAppResponseReader::Error::Type::kParserInternalError:
      return base::unexpected(
          IsolatedWebAppReaderRegistry::ReadResponseHeadError::
              kResponseHeadParserInternalError);
    case IsolatedWebAppResponseReader::Error::Type::kFormatError:
      return base::unexpected(
          IsolatedWebAppReaderRegistry::ReadResponseHeadError::
              kResponseHeadParserFormatError);
    case IsolatedWebAppResponseReader::Error::Type::kResponseNotFound:
      return base::unexpected(
          IsolatedWebAppReaderRegistry::ReadResponseHeadError::
              kResponseNotFoundError);
    case IsolatedWebAppResponseReader::Error::Type::kNotTrusted:
      return base::unexpected(
          IsolatedWebAppReaderRegistry::ReadResponseHeadError::kAppNotTrusted);
  }
}

void CloseReader(std::unique_ptr<IsolatedWebAppResponseReader> reader,
                 base::OnceClosure callback) {
  IsolatedWebAppResponseReader* raw_reader = reader.get();
  base::OnceClosure delete_callback =
      base::DoNothingWithBoundArgs(std::move(reader));
  raw_reader->Close(std::move(callback).Then(std::move(delete_callback)));
}

}  // namespace

class IsolatedWebAppReaderRegistry::Cache {
 public:
  using Key = base::FilePath;

  class PendingState {
   public:
    PendingState() = default;
    ~PendingState() = default;

    PendingState(const PendingState&) = delete;
    PendingState& operator=(const PendingState&) = delete;
    PendingState(PendingState&&) = default;
    PendingState& operator=(PendingState&&) = default;

    void AddRequest(
        const network::ResourceRequest& resource_request,
        IsolatedWebAppReaderRegistry::ReadResponseCallback callback) {
      requests_.emplace_back(resource_request, std::move(callback));
    }

    std::vector<std::pair<network::ResourceRequest,
                          IsolatedWebAppReaderRegistry::ReadResponseCallback>>
    TakeRequests() {
      return std::move(requests_);
    }

    bool IsCloseRequested() const { return !!close_callback_; }

    void SetCloseCallback(base::OnceClosure callback) {
      CHECK(!close_callback_);
      close_callback_ = std::move(callback);
    }

    base::OnceClosure TakeCloseCallback() { return std::move(close_callback_); }

   private:
    std::vector<std::pair<network::ResourceRequest,
                          IsolatedWebAppReaderRegistry::ReadResponseCallback>>
        requests_;
    base::OnceClosure close_callback_;
  };

  class ReadyState {
   public:
    explicit ReadyState(std::unique_ptr<IsolatedWebAppResponseReader> reader)
        : reader_(std::move(reader)), last_access_(base::TimeTicks::Now()) {}
    ~ReadyState() = default;

    ReadyState(const ReadyState&) = delete;
    ReadyState& operator=(const ReadyState&) = delete;
    ReadyState(ReadyState&&) = default;
    ReadyState& operator=(ReadyState&&) = default;

    IsolatedWebAppResponseReader& GetReader() {
      last_access_ = base::TimeTicks::Now();
      return *reader_;
    }

    std::unique_ptr<IsolatedWebAppResponseReader> StealReader() {
      return std::move(reader_);
    }

    base::TimeTicks last_access() const { return last_access_; }

   private:
    std::unique_ptr<IsolatedWebAppResponseReader> reader_;
    base::TimeTicks last_access_;
  };

  using Entry = std::variant<PendingState, ReadyState>;

  Cache() = default;
  ~Cache() = default;

  Cache(Cache&& other) = delete;
  Cache& operator=(Cache&& other) = delete;

  auto find(const Key& key) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return cache_.find(key);
  }

  auto emplace(auto&&... args) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto result = cache_.emplace(std::forward<decltype(args)>(args)...);
    StartCleanupTimerIfNotRunning();
    return result;
  }

  void erase(base::flat_map<Key, Entry>::iterator iterator) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    cache_.erase(iterator);
    StopCleanupTimerIfCacheIsEmpty();
  }

  auto begin() { return cache_.begin(); }
  auto end() { return cache_.end(); }
  auto begin() const { return cache_.begin(); }
  auto end() const { return cache_.end(); }

  bool IsCleanupTimerRunningForTesting() const {
    return cleanup_timer_.IsRunning();
  }

 private:
  void StartCleanupTimerIfNotRunning() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!cache_.empty());
    if (cleanup_timer_.IsRunning()) {
      return;
    }
    cleanup_timer_.Start(
        FROM_HERE, kCleanupInterval,
        base::BindRepeating(&Cache::CleanupOldEntries,
                            // It is safe to use `base::Unretained` here,
                            // because `cache_cleanup_timer_` will be deleted
                            // before `this` is deleted.
                            base::Unretained(this)));
  }

  void StopCleanupTimerIfCacheIsEmpty() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (cache_.empty()) {
      cleanup_timer_.Stop();
    }
  }

  void CleanupOldEntries() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::TimeTicks now = base::TimeTicks::Now();
    base::EraseIf(cache_, [&now](std::pair<Key, Entry>& entry_pair) {
      auto& cache_entry = entry_pair.second;
      if (auto* ready_state = std::get_if<ReadyState>(&cache_entry)) {
        if (now - ready_state->last_access() > kCleanupInterval) {
          CloseReader(ready_state->StealReader(), base::DoNothing());
          return true;
        }
      }
      return false;
    });
    StopCleanupTimerIfCacheIsEmpty();
  }

  base::flat_map<Key, Entry> cache_;
  base::RepeatingTimer cleanup_timer_;
  SEQUENCE_CHECKER(sequence_checker_);
};

IsolatedWebAppReaderRegistry::IsolatedWebAppReaderRegistry(
    content::BrowserContext* browser_context,
    std::unique_ptr<IsolatedWebAppResponseReaderFactory> reader_factory)
    : browser_context_(*browser_context),
      reader_factory_(std::move(reader_factory)),
      cache_(std::make_unique<Cache>()) {
  if (auto* provider = IwaClient::GetInstance()->GetRuntimeDataProvider()) {
    key_provider_observation_.Observe(provider);
  }
}

IsolatedWebAppReaderRegistry::~IsolatedWebAppReaderRegistry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IsolatedWebAppReaderRegistry::ReadResponse(
    const base::FilePath& web_bundle_path,
    bool dev_mode,
    const web_package::SignedWebBundleId& web_bundle_id,
    const network::ResourceRequest& resource_request,
    ReadResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!web_bundle_id.is_for_proxy_mode());

  Cache::Key cache_key = web_bundle_path;

  auto cache_entry_it = cache_->find(cache_key);
  if (cache_entry_it != cache_->end()) {
    std::visit(
        absl::Overload{
            [&](Cache::PendingState& pending) {
              base::UmaHistogramEnumeration(kReaderCacheStateHistogramName,
                                            ReaderCacheState::kCachedPending);
              pending.AddRequest(resource_request, std::move(callback));
            },
            [&](Cache::ReadyState& ready) {
              base::UmaHistogramEnumeration(kReaderCacheStateHistogramName,
                                            ReaderCacheState::kCachedReady);
              DoReadResponse(ready.GetReader(), resource_request,
                             std::move(callback));
            }},
        cache_entry_it->second);
    return;
  }

  // Entry does not exist, create a new pending entry.
  base::UmaHistogramEnumeration(kReaderCacheStateHistogramName,
                                ReaderCacheState::kNotCached);

  Cache::PendingState new_pending_state;
  new_pending_state.AddRequest(resource_request, std::move(callback));

  cache_->emplace(cache_key, std::move(new_pending_state));

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, signatures are only verified at install-time. The location of
  // the installed bundles inside of cryptohome is deemed secure enough to not
  // necessitate re-verification of signatures once per session.
  bool skip_signature_verification = true;
#else
  // If we already verified the signatures of this Signed Web Bundle during
  // the current browser session, we trust that the Signed Web Bundle has not
  // been tampered with and don't re-verify signatures.
  bool skip_signature_verification = verified_files_.contains(web_bundle_path);
#endif

  IsolatedWebAppResponseReaderFactory::Flags flags;
  if (dev_mode) {
    flags.Put(IsolatedWebAppResponseReaderFactory::Flag::kDevModeBundle);
  }
  if (skip_signature_verification) {
    flags.Put(
        IsolatedWebAppResponseReaderFactory::Flag::kSkipSignatureVerification);
  }

  reader_factory_->CreateResponseReader(
      web_bundle_path, web_bundle_id, flags,
      base::BindOnce(&IsolatedWebAppReaderRegistry::OnResponseReaderCreated,
                     // `base::Unretained` can be used here since `this` owns
                     // `reader_factory`.
                     base::Unretained(this), web_bundle_path, web_bundle_id));
}

// Processes a key data change event and queues close requests for readers
// corresponding to bundles that might be affected by key rotation. These
// requests will be fulfilled once the app closes.
void IsolatedWebAppReaderRegistry::OnRuntimeDataChanged() {
  std::vector<std::pair<base::FilePath, web_package::SignedWebBundleId>>
      affected_ready_readers;
  for (auto& [path, entry] : *cache_) {
    std::visit(
        absl::Overload{
            [&](Cache::PendingState& pending) {
              // If this reader is affected too, it will be closed in
              // OnResponseReaderCreated() during the identity check.
            },
            [&](Cache::ReadyState& ready) {
              if (!web_package::IdentityValidator::GetInstance()
                       ->ValidateWebBundleIdentity(
                           ready.GetReader().GetIntegrityBlock())
                       .has_value()) {
                affected_ready_readers.emplace_back(
                    path,
                    ready.GetReader().GetIntegrityBlock().web_bundle_id());
              }
            }},
        entry);
  }

  for (const auto& [path, web_bundle_id] : affected_ready_readers) {
    IwaClient::GetInstance()->RunWhenAppCloses(
        &browser_context_.get(), web_bundle_id,
        base::BindOnce(&IsolatedWebAppReaderRegistry::ClearCacheForPath,
                       weak_ptr_factory_.GetWeakPtr(), path,
                       base::DoNothing()));
  }
}

void IsolatedWebAppReaderRegistry::ClearCacheForPath(
    const base::FilePath& web_bundle_path,
    base::OnceClosure callback) {
  auto cache_entry_it = cache_->find(web_bundle_path);
  if (cache_entry_it == cache_->end()) {
    std::move(callback).Run();
    return;
  }

  std::visit(absl::Overload{[&](Cache::PendingState& pending) {
                              pending.SetCloseCallback(std::move(callback));
                            },
                            [&](Cache::ReadyState& ready) {
                              CloseReader(ready.StealReader(),
                                          std::move(callback));
                              cache_->erase(cache_entry_it);
                            }},
             cache_entry_it->second);
}

void IsolatedWebAppReaderRegistry::OnResponseReaderCreated(
    const base::FilePath& web_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    base::expected<std::unique_ptr<IsolatedWebAppResponseReader>,
                   UnusableSwbnFileError> maybe_reader) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The entry must exist in the cache and be in the pending state.
  auto cache_entry_it = cache_->find(web_bundle_path);
  CHECK(cache_entry_it != cache_->end());
  auto& pending_state =
      CHECK_DEREF(std::get_if<Cache::PendingState>(&cache_entry_it->second));

  auto requests = pending_state.TakeRequests();

  ASSIGN_OR_RETURN(
      auto reader, std::move(maybe_reader),
      [&](const UnusableSwbnFileError& error) {
        // Handles the case where `reader` is not defined (due to an error on a
        // lower layer).
        const auto read_response_error = ReadResponseError::ForError(error);
        for (auto& [resource_request, callback] : requests) {
          std::move(callback).Run(base::unexpected(read_response_error));
        }
        cache_->erase(cache_entry_it);
      });

  auto should_proceed_with_response_reading =
      [&]() -> base::expected<void, std::string> {
    if (pending_state.IsCloseRequested()) {
      return base::unexpected("The bundle is waiting to close");
    }
    // This check is realistically somewhat excessive (given that the same
    // identity check is performed during .swbn file parsing). However, given
    // that the state of the key dist component might have changed in the
    // meantime, it's better to double-check this.
    RETURN_IF_ERROR(
        web_package::IdentityValidator::GetInstance()
            ->ValidateWebBundleIdentity(reader->GetIntegrityBlock()));

    return base::ok();
  }();

  if (!should_proceed_with_response_reading.has_value()) {
    // Handles the case where `reader` is defined, but has to be closed due to
    // either an earlier ask or to identity mismatch.
    const auto read_response_error = ReadResponseError::ForOtherError(
        should_proceed_with_response_reading.error());

    for (auto& [resource_request, callback] : requests) {
      std::move(callback).Run(base::unexpected(read_response_error));
    }
    CloseReader(std::move(reader), pending_state.TakeCloseCallback());
    cache_->erase(cache_entry_it);
    return;
  }

  // The `SignedWebBundleReader` is now ready to read responses. Inform all
  // consumers that were waiting for this `SignedWebBundleReader` to become
  // available.
  verified_files_.insert(cache_entry_it->first);
  auto& ready_state =
      cache_entry_it->second.emplace<Cache::ReadyState>(std::move(reader));
  for (auto& [resource_request, callback] : requests) {
    DoReadResponse(ready_state.GetReader(), resource_request,
                   std::move(callback));
  }
}

void IsolatedWebAppReaderRegistry::DoReadResponse(
    IsolatedWebAppResponseReader& reader,
    network::ResourceRequest resource_request,
    ReadResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Remove query parameters from the request URL, if it has any.
  // Resources within Signed Web Bundles used for Isolated Web Apps never have
  // username, password, or fragment, just like resources within Signed Web
  // Bundles and normal Web Bundles. Removing these from request URLs is done by
  // the `SignedWebBundleReader`. However, in addition, resources in Signed Web
  // Bundles used for Isolated Web Apps can also never have query parameters,
  // which we need to remove here.
  //
  // Conceptually, we treat the resources in Signed Web Bundles for Isolated Web
  // Apps more like files served by a file server (which also strips query
  // parameters before looking up the file), and not like HTTP exchanges as they
  // are used for Signed Exchanges (SXG).
  if (resource_request.url.has_query()) {
    GURL::Replacements replacements;
    replacements.ClearQuery();
    resource_request.url = resource_request.url.ReplaceComponents(replacements);
  }

  reader.ReadResponse(
      resource_request,
      base::BindOnce(
          &IsolatedWebAppReaderRegistry::OnResponseRead,
          // `base::Unretained` can be used here since `this` owns `reader`.
          base::Unretained(this), std::move(callback)));
}

void IsolatedWebAppReaderRegistry::OnResponseRead(
    ReadResponseCallback callback,
    base::expected<IsolatedWebAppResponseReader::Response,
                   IsolatedWebAppResponseReader::Error> response) {
  base::expected<void, IsolatedWebAppReaderRegistry::ReadResponseHeadError>
      response_status = ToReadResponseHeadError(response);
  UmaLogExpectedStatus("WebApp.Isolated.ReadResponseHead", response_status);

  std::move(callback).Run(std::move(response).transform_error(
      static_cast<ReadResponseError (*)(
          const IsolatedWebAppResponseReader::Error&)>(
          &ReadResponseError::ForError)));
}

// static
IsolatedWebAppReaderRegistry::ReadResponseError
IsolatedWebAppReaderRegistry::ReadResponseError::ForError(
    const UnusableSwbnFileError& error) {
  return ForOtherError(
      IsolatedWebAppResponseReaderFactory::ErrorToString(error));
}

// static
IsolatedWebAppReaderRegistry::ReadResponseError
IsolatedWebAppReaderRegistry::ReadResponseError::ForError(
    const IsolatedWebAppResponseReader::Error& error) {
  switch (error.type) {
    case IsolatedWebAppResponseReader::Error::Type::kParserInternalError:
    case IsolatedWebAppResponseReader::Error::Type::kFormatError:
    case IsolatedWebAppResponseReader::Error::Type::kNotTrusted:
      return ForOtherError(base::StringPrintf(
          "Failed to parse response head: %s", error.message.c_str()));
    case IsolatedWebAppResponseReader::Error::Type::kResponseNotFound:
      return ForResponseNotFound(base::StringPrintf(
          "Failed to read response from Signed Web Bundle: %s",
          error.message.c_str()));
  }
}

bool IsolatedWebAppReaderRegistry::IsCleanupTimerRunningForTesting() const {
  return cache_->IsCleanupTimerRunningForTesting();
}

}  // namespace web_app
