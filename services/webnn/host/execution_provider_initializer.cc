// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/host/execution_provider_initializer.h"

#include <appmodel.h>
#include <wrl.h>

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/concurrent_closures.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_hstring.h"
#include "services/webnn/public/cpp/execution_providers_info.h"
#include "services/webnn/public/cpp/platform_functions_win.h"
#include "services/webnn/public/cpp/win_app_runtime_package_info.h"
#include "services/webnn/webnn_switches.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/Microsoft.Windows.AI.MachineLearning.h"

namespace webnn {

namespace {

namespace abi_winml = ::ABI::Microsoft::Windows::AI::MachineLearning;

using ::ABI::Windows::Foundation::AsyncStatus;
using EnsureReadyAsyncOp =
    __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double;
using EnsureReadyCompletedHandler =
    __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double;

// A flat map with execution provider (EP) name as the key and package info as
// the value.
using EpPackageInfoMap = base::flat_map<std::string, mojom::EpPackageInfoPtr>;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ExecutionProviderStatusUma)
enum class ExecutionProviderStatusUma {
  kUnknown = 0,
  kEpVersionTooLow = 1,
  kNotInstalled = 2,
  kEnsureReadyFailed = 3,
  kReadyForUse = 4,

  kMaxValue = kReadyForUse,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webnn/enums.xml:ExecutionProviderStatusUma)

void RecordEpStatus(std::string_view ep_name,
                    ExecutionProviderStatusUma status) {
  constexpr std::string_view kWebnnHistogramPrefix = "WebNN.ORT.";
  constexpr std::string_view kWebnnHistogramSuffix = ".Status";
  base::UmaHistogramEnumeration(
      base::StrCat({kWebnnHistogramPrefix, ep_name, kWebnnHistogramSuffix}),
      status);
}

bool operator<(const PACKAGE_VERSION& a, const PACKAGE_VERSION& b) {
  if (a.Major != b.Major) {
    return a.Major < b.Major;
  }
  if (a.Minor != b.Minor) {
    return a.Minor < b.Minor;
  }
  if (a.Build != b.Build) {
    return a.Build < b.Build;
  }
  return a.Revision < b.Revision;
}

std::string VersionToString(const PACKAGE_VERSION& version) {
  constexpr std::string_view kPackageVersionFormat = "%u.%u.%u.%u";
  return base::StringPrintf(kPackageVersionFormat, version.Major, version.Minor,
                            version.Build, version.Revision);
}

auto CloneMap(const EpPackageInfoMap& map) {
  std::vector<std::pair<std::string, mojom::EpPackageInfoPtr>> cloned_entries;
  cloned_entries.reserve(map.size());
  std::ranges::for_each(map, [&cloned_entries](const auto& pair) {
    cloned_entries.emplace_back(pair.first, pair.second.Clone());
  });
  return base::flat_map(std::move(cloned_entries));
}

// Retrieves the name of the EP.
std::string GetProviderName(abi_winml::IExecutionProvider* provider) {
  base::win::ScopedHString name(nullptr);
  HRESULT hr =
      provider->get_Name(base::win::ScopedHString::Receiver(name).get());
  CHECK_EQ(hr, S_OK);
  return name.GetAsUTF8();
}

// Activates the EP Catalog and retrieves all available EPs.
std::vector<Microsoft::WRL::ComPtr<abi_winml::IExecutionProvider>>
ActivateCatalogAndGetAvailableEps() {
  HRESULT hr = S_OK;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const bool ignore_ep_blocklist =
      command_line->HasSwitch(switches::kWebNNOrtIgnoreEpBlocklist);

  Microsoft::WRL::ComPtr<abi_winml::IExecutionProviderCatalogStatics>
      catalog_statics;
  {
    // `RoGetActivationFactory()` below loads Windows ML dlls for activating the
    // EP Catalog API.
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    hr = base::win::RoGetActivationFactory(
        base::win::ScopedHString::Create(
            RuntimeClass_Microsoft_Windows_AI_MachineLearning_ExecutionProviderCatalog)
            .get(),
        IID_PPV_ARGS(&catalog_statics));
  }
  if (FAILED(hr)) {
    PLOG(WARNING) << "[WebNN] RoGetActivationFactory() failed.";
    return {};
  }

  // Get the IExecutionProviderCatalog interface.
  Microsoft::WRL::ComPtr<abi_winml::IExecutionProviderCatalog> catalog;
  hr = catalog_statics->GetDefault(&catalog);
  if (FAILED(hr)) {
    PLOG(WARNING) << "[WebNN] catalog_statics->GetDefault() failed.";
    return {};
  }

  base::win::ScopedCoMem<abi_winml::IExecutionProvider*> comem_providers;
  uint32_t providers_count = 0;
  hr = catalog->FindAllProviders(&providers_count, &comem_providers);
  if (FAILED(hr)) {
    PLOG(WARNING) << "[WebNN] catalog->FindAllProviders() failed.";
    return {};
  }
  // SAFETY: `comem_providers` is allocated by WinRT and guaranteed to be
  // valid.
  auto provider_span =
      UNSAFE_BUFFERS(base::span<abi_winml::IExecutionProvider*>(
          comem_providers.get(), providers_count));

  std::vector<Microsoft::WRL::ComPtr<abi_winml::IExecutionProvider>> providers;
  for (auto provider_ptr : provider_span) {
    // Scope `provider_ptr` to avoid memory leak.
    Microsoft::WRL::ComPtr<abi_winml::IExecutionProvider> provider;
    provider.Attach(provider_ptr);

    const std::string provider_name = GetProviderName(provider.Get());
    const auto known_it = kKnownEPs.find(provider_name);
    // If the name is not recognized in `kKnownEPs`, skip this EP.
    if (known_it == kKnownEPs.end()) {
      continue;
    }
    if (!ignore_ep_blocklist && !known_it->second.enabled) {
      continue;
    }
    providers.push_back(provider);
  }

  return providers;
}

std::optional<std::pair<std::string, mojom::EpPackageInfoPtr>>
QueryPackageInfoFromProvider(abi_winml::IExecutionProvider* provider,
                             EnsureReadyAsyncOp* ensure_op) {
  Microsoft::WRL::ComPtr<IAsyncInfo> async_info;
  HRESULT hr = ensure_op->QueryInterface(IID_PPV_ARGS(&async_info));
  CHECK_EQ(hr, S_OK);

  std::string ep_name = GetProviderName(provider);

  AsyncStatus status;
  hr = async_info->get_Status(&status);
  CHECK_EQ(hr, S_OK);
  if (status != AsyncStatus::Completed) {
    RecordEpStatus(ep_name, ExecutionProviderStatusUma::kUnknown);

    LOG(WARNING) << "[WebNN] EnsureReadyAsync() didn't complete for "
                 << ep_name;
    return std::nullopt;
  }

  Microsoft::WRL::ComPtr<abi_winml::IExecutionProviderReadyResult> ready_result;
  hr = ensure_op->GetResults(&ready_result);
  CHECK_EQ(hr, S_OK);

  abi_winml::ExecutionProviderReadyResultState ready_state;
  hr = ready_result->get_Status(&ready_state);
  switch (ready_state) {
    case abi_winml::ExecutionProviderReadyResultState_Success: {
      base::win::ScopedHString ep_path(nullptr);
      hr = provider->get_LibraryPath(
          base::win::ScopedHString::Receiver(ep_path).get());
      CHECK_EQ(hr, S_OK);
      CHECK(ep_path.is_valid());

      Microsoft::WRL::ComPtr<ABI::Windows::ApplicationModel::IPackageId>
          package_id;
      hr = provider->get_PackageId(&package_id);
      CHECK_EQ(hr, S_OK);

      base::win::ScopedHString family_name(nullptr);
      hr = package_id->get_FamilyName(
          base::win::ScopedHString::Receiver(family_name).get());
      CHECK_EQ(hr, S_OK);
      CHECK(family_name.is_valid());

      ABI::Windows::ApplicationModel::PackageVersion abi_package_version;
      hr = package_id->get_Version(&abi_package_version);
      CHECK_EQ(hr, S_OK);

      PACKAGE_VERSION package_version = {
          .Major = abi_package_version.Major,
          .Minor = abi_package_version.Minor,
          .Build = abi_package_version.Build,
          .Revision = abi_package_version.Revision,
      };

      const PACKAGE_VERSION& min_package_version =
          kKnownEPs.find(ep_name)->second.min_package_version;
      if (package_version < min_package_version) {
        RecordEpStatus(ep_name, ExecutionProviderStatusUma::kEpVersionTooLow);

        LOG(WARNING) << "[WebNN] Found [" << ep_name << "] package version: "
                     << VersionToString(package_version)
                     << " is lower than the minimum required version: "
                     << VersionToString(min_package_version);
        return std::nullopt;
      }

      RecordEpStatus(ep_name, ExecutionProviderStatusUma::kReadyForUse);

      return std::make_pair(
          std::move(ep_name),
          mojom::EpPackageInfo::New(std::wstring(family_name.Get()),
                                    std::move(package_version),
                                    base::FilePath(ep_path.Get())));
    }
    case abi_winml::ExecutionProviderReadyResultState_Failure: {
      RecordEpStatus(ep_name, ExecutionProviderStatusUma::kEnsureReadyFailed);

      HRESULT extended_error;
      hr = ready_result->get_ExtendedError(&extended_error);
      CHECK_EQ(hr, S_OK);

      base::win::ScopedHString diagnostic_text(nullptr);
      hr = ready_result->get_DiagnosticText(
          base::win::ScopedHString::Receiver(diagnostic_text).get());
      CHECK_EQ(hr, S_OK);

      LOG(WARNING) << "[WebNN] [" << ep_name
                   << "] failed to get ready. Extended error: " << std::hex
                   << extended_error
                   << ", diagnostic text: " << diagnostic_text.GetAsUTF8();
      return std::nullopt;
    }
    case abi_winml::ExecutionProviderReadyResultState_InProgress: {
      LOG(FATAL)
          << "[WebNN] [" << ep_name
          << "] is still in progress after EnsureReadyAsync() completed.";
    }
  }
}

void EnsureExecutionProviderReadyAsync(
    Microsoft::WRL::ComPtr<abi_winml::IExecutionProvider> provider,
    base::OnceCallback<
        void(std::optional<std::pair<std::string, mojom::EpPackageInfoPtr>>)>
        callback) {
  Microsoft::WRL::ComPtr<EnsureReadyAsyncOp> ensure_op;
  HRESULT hr = provider->EnsureReadyAsync(&ensure_op);
  if (FAILED(hr)) {
    PLOG(WARNING) << "[WebNN] EnsureReadyAsync() failed for "
                  << GetProviderName(provider.Get());
    std::move(callback).Run(std::nullopt);
    return;
  }

  ensure_op->put_Completed(
      Microsoft::WRL::Callback<EnsureReadyCompletedHandler>(
          [provider,
           callback = base::BindPostTaskToCurrentDefault(std::move(callback))](
              EnsureReadyAsyncOp* ensure_op, AsyncStatus status) mutable {
            std::move(callback).Run(
                QueryPackageInfoFromProvider(provider.Get(), ensure_op));
            return S_OK;
          })
          .Get());
}

// A singleton class that manages installation and initialization of EP
// packages using the EP Catalog API of Windows App Runtime, ensuring all
// required EPs are ready for use.
class ExecutionProviderInitializer {
 public:
  static ExecutionProviderInitializer* GetInstance() {
    static base::NoDestructor<ExecutionProviderInitializer> instance;
    return instance.get();
  }

  ~ExecutionProviderInitializer() = delete;
  ExecutionProviderInitializer(const ExecutionProviderInitializer&) = delete;
  ExecutionProviderInitializer& operator=(const ExecutionProviderInitializer&) =
      delete;

  void EnsureExecutionProvidersReady(
      base::OnceCallback<void(EpPackageInfoMap)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    switch (state_) {
      case State::kEpCatalogNotActivated: {
        // Tries to activate the EP Catalog, if succeeded, queues the callback
        // to wait for the EPs to get ready, otherwise invokes the callback with
        // an empty map immediately.
        if (TryActivateEPCatalog()) {
          state_ = State::kEpCatalogActivated;
          pending_callbacks_.push(std::move(callback));
        } else {
          std::move(callback).Run({});
        }
        return;
      }
      case State::kEpCatalogActivated: {
        pending_callbacks_.push(std::move(callback));
        return;
      }
      case State::kEpsEnsured: {
        std::move(callback).Run(CloneMap(ep_package_info_map_));
        return;
      }
    }
  }

 private:
  friend class base::NoDestructor<ExecutionProviderInitializer>;

  ExecutionProviderInitializer() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // Activates the EP Catalog if not already activated.
  bool TryActivateEPCatalog() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Initializes the dependency on the runtime package to ensure the EP
    // Catalog can be used.
    if (InitializePackageDependencyForProcess(kWinAppRuntimePackageFamilyName,
                                              kWinAppRuntimePackageMinVersion)
            .empty()) {
      return false;
    }

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&ActivateCatalogAndGetAvailableEps),
        base::BindOnce(&ExecutionProviderInitializer::Initialize,
                       base::Unretained(this)));

    return true;
  }

  // Initializes the EPs retrieved from the catalog and triggers installation of
  // their packages.
  void Initialize(
      std::vector<Microsoft::WRL::ComPtr<abi_winml::IExecutionProvider>>
          providers) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Checks the ready state of each provider and ensures they reach the ready
    // state.
    //
    // Providers in the "not ready" state are already installed, so
    // `EnsureExecutionProviderReadyAsync` is expected to complete quickly.
    // `concurrent_closures` will wait for these providers to get ready
    // before invoking `OnInitialize()`, which blocks WebNN context creation.
    //
    // Providers in the "not present" state are not yet installed.
    // `EnsureExecutionProviderReadyAsync` will trigger the package download and
    // installation, which takes longer time to complete. This installation runs
    // on a background thread and does NOT block WebNN context creation.
    base::ConcurrentClosures concurrent_closures;
    for (const auto& provider : providers) {
      abi_winml::ExecutionProviderReadyState ready_state;
      HRESULT hr = provider->get_ReadyState(&ready_state);
      CHECK_EQ(hr, S_OK);

      std::string ep_name = GetProviderName(provider.Get());

      switch (ready_state) {
        case abi_winml::ExecutionProviderReadyState_Ready: {
          LOG(FATAL)
              << "[WebNN] [" << ep_name
              << "] is already in ready state before `EnsureReadyAsync()` "
                 "is called.";
        }
        case abi_winml::ExecutionProviderReadyState_NotReady: {
          EnsureExecutionProviderReadyAsync(
              provider,
              base::BindOnce(
                  [](base::OnceClosure closure,
                     std::optional<std::pair<
                         std::string, mojom::EpPackageInfoPtr>> package_info) {
                    if (package_info.has_value()) {
                      auto* instance =
                          ExecutionProviderInitializer::GetInstance();
                      instance->AddExecutionProviderPackageInfo(
                          std::move(*package_info));
                    }
                    std::move(closure).Run();
                  },
                  concurrent_closures.CreateClosure()));
          break;
        }
        case abi_winml::ExecutionProviderReadyState_NotPresent: {
          RecordEpStatus(ep_name, ExecutionProviderStatusUma::kNotInstalled);

          EnsureExecutionProviderReadyAsync(
              provider, base::BindOnce(
                            [](std::optional<
                                std::pair<std::string, mojom::EpPackageInfoPtr>>
                                   package_info) {
                              if (package_info.has_value()) {
                                auto* instance =
                                    ExecutionProviderInitializer::GetInstance();
                                instance->AddExecutionProviderPackageInfo(
                                    std::move(*package_info));
                              }
                            }));
          break;
        }
      }
    }

    std::move(concurrent_closures)
        .Done(base::BindOnce(&ExecutionProviderInitializer::OnInitialize,
                             base::Unretained(this)));
  }

  // Called when the installed execution providers are ensured ready and their
  // package info are cached. Invokes all the pending callbacks.
  void OnInitialize() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    state_ = State::kEpsEnsured;

    while (!pending_callbacks_.empty()) {
      std::move(pending_callbacks_.front()).Run(CloneMap(ep_package_info_map_));
      pending_callbacks_.pop();
    }
  }

  void AddExecutionProviderPackageInfo(
      std::pair<std::string, mojom::EpPackageInfoPtr> package_info) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    ep_package_info_map_.insert(std::move(package_info));
  }

  // Cached package info of EPs that are ready for use.
  EpPackageInfoMap ep_package_info_map_;

  // Pending callbacks to be invoked once initialization is complete.
  base::queue<base::OnceCallback<void(EpPackageInfoMap)>> pending_callbacks_;

  enum class State {
    // The EP Catalog is not activated yet.
    kEpCatalogNotActivated,
    // The EP Catalog is activated but the EPs are not ready.
    kEpCatalogActivated,
    // The already installed EPs are ensured to be ready.
    kEpsEnsured,
  };
  State state_ = State::kEpCatalogNotActivated;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

void EnsureExecutionProvidersReady(
    base::OnceCallback<void(EpPackageInfoMap)> callback) {
  auto* instance = ExecutionProviderInitializer::GetInstance();
  instance->EnsureExecutionProvidersReady(std::move(callback));
}

}  // namespace webnn
