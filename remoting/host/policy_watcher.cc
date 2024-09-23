// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most of this code is copied from:
//   src/chrome/browser/policy/asynchronous_policy_loader.{h,cc}

#include "remoting/host/policy_watcher.h"

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/async_policy_loader.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"
#include "remoting/base/port_range.h"

#if !defined(NDEBUG)
#include "base/json/json_reader.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "components/policy/core/common/policy_loader_win.h"
#elif BUILDFLAG(IS_APPLE)
#include "base/apple/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/policy/core/common/policy_loader_mac.h"
#include "components/policy/core/common/preferences_mac.h"
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
#include "components/policy/core/common/config_dir_policy_loader.h"
#include "components/policy/core/common/policy_paths.h"  // nogncheck
#endif

namespace remoting {

namespace key = ::policy::key;

namespace {

#if BUILDFLAG(IS_WIN)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr wchar_t kChromePolicyKey[] = L"SOFTWARE\\Policies\\Google\\Chrome";
#else
constexpr wchar_t kChromePolicyKey[] = L"SOFTWARE\\Policies\\Chromium";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_WIN)

// Copies all policy values from one dictionary to another, using values from
// |default_values| if they are not set in |from|.
base::Value::Dict CopyValuesAndAddDefaults(
    const base::Value::Dict& from,
    const base::Value::Dict& default_values) {
  base::Value::Dict to(default_values.Clone());
  for (auto i : default_values) {
    // If the policy isn't in |from|, use the default.
    const base::Value* value = from.FindByDottedPath(i.first);
    if (!value) {
      continue;
    }

    CHECK(value->type() == i.second.type() || value->is_none() ||
          i.second.is_none());
    to.Set(i.first, value->Clone());
  }

  return to;
}

policy::PolicyNamespace GetPolicyNamespace() {
  return policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());
}

std::unique_ptr<policy::SchemaRegistry> CreateSchemaRegistry() {
  // TODO(lukasza): Schema below should ideally only cover Chromoting-specific
  // policies (expecting perf and maintanability improvement, but no functional
  // impact).
  policy::Schema schema = policy::Schema::Wrap(policy::GetChromeSchemaData());

  auto schema_registry = std::make_unique<policy::SchemaRegistry>();
  schema_registry->RegisterComponent(GetPolicyNamespace(), schema);
  return schema_registry;
}

base::Value::Dict CopyChromotingPoliciesIntoDictionary(
    const policy::PolicyMap& current) {
  const char kPolicyNameSubstring[] = "RemoteAccessHost";
  base::Value::Dict policy_dict;
  for (const auto& entry : current) {
    const std::string& key = entry.first;
    // |value_unsafe| is used due to multiple policy types being handled.
    const base::Value* value = entry.second.value_unsafe();

    // Copying only Chromoting-specific policies helps avoid false alarms
    // raised by NormalizePolicies below (such alarms shutdown the host).
    // TODO(lukasza): Removing this somewhat brittle filtering will be possible
    //                after having separate, Chromoting-specific schema.
    if (key.find(kPolicyNameSubstring) != std::string::npos) {
      policy_dict.Set(key, value->Clone());
    }
  }

  return policy_dict;
}

// Takes a dictionary containing only 1) recognized policy names and 2)
// well-typed policy values and further verifies policy contents.
bool VerifyWellformedness(const base::Value::Dict& changed_policies) {
  // Verify UdpPortRange policy.
  const std::string* udp_port_range_string =
      changed_policies.FindString(policy::key::kRemoteAccessHostUdpPortRange);
  PortRange udp_port_range;
  if (udp_port_range_string) {
    if (!PortRange::Parse(*udp_port_range_string, &udp_port_range)) {
      return false;
    }
  }

  // Report that all the policies were well-formed.
  return true;
}

}  // namespace

void PolicyWatcher::StartWatching(
    const PolicyUpdatedCallback& policy_updated_callback,
    const PolicyErrorCallback& policy_error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!policy_updated_callback.is_null());
  DCHECK(!policy_error_callback.is_null());
  DCHECK(policy_updated_callback_.is_null());

  policy_updated_callback_ = policy_updated_callback;
  policy_error_callback_ = policy_error_callback;

  // Listen for future policy changes.
  policy_service_->AddObserver(policy::POLICY_DOMAIN_CHROME, this);

  // Process current policy state.
  if (policy_service_->IsInitializationComplete(policy::POLICY_DOMAIN_CHROME)) {
    OnPolicyServiceInitialized(policy::POLICY_DOMAIN_CHROME);
  }
}

base::Value::Dict PolicyWatcher::GetEffectivePolicies() {
  return effective_policies_.Clone();
}

base::Value::Dict PolicyWatcher::GetPlatformPolicies() {
  return platform_policies_.Clone();
}

base::Value::Dict PolicyWatcher::GetDefaultPolicies() {
  base::Value::Dict result;
  result.Set(key::kRemoteAccessHostFirewallTraversal, true);
  result.Set(key::kRemoteAccessHostClientDomainList, base::Value::List());
  result.Set(key::kRemoteAccessHostDomainList, base::Value::List());
  result.Set(key::kRemoteAccessHostAllowRelayedConnection, true);
  result.Set(key::kRemoteAccessHostUdpPortRange, "");
  result.Set(key::kRemoteAccessHostClipboardSizeBytes, -1);
  result.Set(key::kRemoteAccessHostAllowRemoteSupportConnections, true);
#if BUILDFLAG(IS_CHROMEOS)
  result.Set(key::kRemoteAccessHostAllowEnterpriseRemoteSupportConnections,
             true);
  result.Set(key::kRemoteAccessHostAllowEnterpriseFileTransfer, false);
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  result.Set(key::kRemoteAccessHostMatchUsername, false);
#endif
#if !BUILDFLAG(IS_CHROMEOS)
  result.Set(key::kRemoteAccessHostRequireCurtain, false);
  result.Set(key::kRemoteAccessHostAllowClientPairing, true);
  result.Set(key::kRemoteAccessHostAllowGnubbyAuth, true);
  result.Set(key::kRemoteAccessHostAllowFileTransfer, true);
  result.Set(key::kRemoteAccessHostAllowUrlForwarding, true);
  result.Set(key::kRemoteAccessHostEnableUserInterface, true);
  result.Set(key::kRemoteAccessHostAllowRemoteAccessConnections, true);
  result.Set(key::kRemoteAccessHostMaximumSessionDurationMinutes, 0);
  result.Set(key::kRemoteAccessHostAllowPinAuthentication, base::Value());
#endif
#if BUILDFLAG(IS_WIN)
  result.Set(key::kRemoteAccessHostAllowUiAccessForRemoteAssistance, false);
#endif
  return result;
}

void PolicyWatcher::SignalPolicyError() {
  effective_policies_.clear();
  platform_policies_.clear();
  policy_error_callback_.Run();
}

PolicyWatcher::PolicyWatcher(
    policy::PolicyService* policy_service,
    std::unique_ptr<policy::PolicyService> owned_policy_service,
    std::unique_ptr<policy::ConfigurationPolicyProvider> owned_policy_provider,
    std::unique_ptr<policy::SchemaRegistry> owned_schema_registry)
    : default_values_(GetDefaultPolicies()),
      owned_schema_registry_(std::move(owned_schema_registry)),
      owned_policy_provider_(std::move(owned_policy_provider)),
      owned_policy_service_(std::move(owned_policy_service)),
      policy_service_(policy_service) {
  DCHECK(policy_service_);
  DCHECK(owned_schema_registry_);
}

PolicyWatcher::~PolicyWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Stop observing |policy_service_| if StartWatching() has been called.
  if (!policy_updated_callback_.is_null()) {
    policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  }

  if (owned_policy_provider_) {
    owned_policy_provider_->Shutdown();
  }
}

const policy::Schema* PolicyWatcher::GetPolicySchema() const {
  return owned_schema_registry_->schema_map()->GetSchema(GetPolicyNamespace());
}

bool PolicyWatcher::NormalizePolicies(base::Value* policy_dict) {
  // Allowing unrecognized policy names allows presence of
  // 1) comments (i.e. JSON of the form: { "_comment": "blah", ... }),
  // 2) policies intended for future/newer versions of the host,
  // 3) policies not supported on all OS-s (i.e. RemoteAccessHostMatchUsername
  //    is not supported on Windows and therefore policy_templates.json omits
  //    schema for this policy on this particular platform).
  auto strategy = policy::SCHEMA_ALLOW_UNKNOWN;

  DCHECK(policy_dict->GetIfDict());

  policy::PolicyErrorPath path;
  std::string error;
  bool changed = false;
  const policy::Schema* schema = GetPolicySchema();
  if (schema->Normalize(policy_dict, strategy, &path, &error, &changed)) {
    if (changed) {
      LOG(WARNING) << "Unknown (unrecognized or unsupported) policy at: "
                   << policy::ErrorPathToString("toplevel", path) << ": "
                   << error;
    }
    HandleDeprecatedPolicies(&policy_dict->GetDict());
    return true;
  } else {
    LOG(ERROR) << "Invalid policy contents at: "
               << policy::ErrorPathToString("toplevel", path) << ": " << error;
    return false;
  }
}

void PolicyWatcher::HandleDeprecatedPolicies(base::Value::Dict* dict) {
  // RemoteAccessHostDomain
  if (dict->Find(policy::key::kRemoteAccessHostDomain)) {
    if (!dict->Find(policy::key::kRemoteAccessHostDomainList)) {
      const std::string* domain =
          dict->FindString(policy::key::kRemoteAccessHostDomain);
      if (domain && !domain->empty()) {
        base::Value::List list;
        list.Append(*domain);
        dict->Set(policy::key::kRemoteAccessHostDomainList, std::move(list));
      }
    }
    dict->Remove(policy::key::kRemoteAccessHostDomain);
  }

  // RemoteAccessHostClientDomain
  if (const std::string* domain =
          dict->FindString(policy::key::kRemoteAccessHostClientDomain)) {
    if (!dict->Find(policy::key::kRemoteAccessHostClientDomainList)) {
      if (!domain->empty()) {
        base::Value::List list;
        list.Append(*domain);
        dict->Set(policy::key::kRemoteAccessHostClientDomainList,
                  std::move(list));
      }
    }
    dict->Remove(policy::key::kRemoteAccessHostClientDomain);
  }
}

base::Value::Dict PolicyWatcher::StoreNewAndReturnChangedPolicies(
    base::Value::Dict new_policies) {
  // Find the changed policies.
  base::Value::Dict changed_policies;
  for (auto iter : new_policies) {
    base::Value* old_policy = effective_policies_.FindByDottedPath(iter.first);
    if (!old_policy || *old_policy != iter.second) {
      changed_policies.Set(iter.first, iter.second.Clone());
    }
  }

  // Save the new policies.
  std::swap(effective_policies_, new_policies);

  return changed_policies;
}

void PolicyWatcher::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                    const policy::PolicyMap& previous,
                                    const policy::PolicyMap& current) {
  base::Value new_policies(CopyChromotingPoliciesIntoDictionary(current));

  // Check for mistyped values and get rid of unknown policies.
  if (!NormalizePolicies(&new_policies)) {
    SignalPolicyError();
    return;
  }

  platform_policies_ = new_policies.GetDict().Clone();

  // Use default values for any missing policies.
  base::Value::Dict filled_policies =
      CopyValuesAndAddDefaults(new_policies.GetDict(), default_values_);

  // Limit reporting to only the policies that were changed.
  base::Value::Dict changed_policies =
      StoreNewAndReturnChangedPolicies(std::move(filled_policies));
  if (changed_policies.empty()) {
    return;
  }

  // Verify that we are calling the callback with valid policies.
  if (!VerifyWellformedness(changed_policies)) {
    SignalPolicyError();
    return;
  }

  // Notify our client of the changed policies.
  policy_updated_callback_.Run(std::move(changed_policies));
}

void PolicyWatcher::OnPolicyServiceInitialized(policy::PolicyDomain domain) {
  policy::PolicyNamespace ns = GetPolicyNamespace();
  const policy::PolicyMap& current = policy_service_->GetPolicies(ns);
  OnPolicyUpdated(ns, current, current);

#if BUILDFLAG(IS_WIN)
  WatchForRegistryChanges();
#endif
}

std::unique_ptr<PolicyWatcher> PolicyWatcher::CreateFromPolicyLoader(
    std::unique_ptr<policy::AsyncPolicyLoader> async_policy_loader) {
  std::unique_ptr<policy::SchemaRegistry> schema_registry =
      CreateSchemaRegistry();
  auto policy_provider = std::make_unique<policy::AsyncPolicyProvider>(
      schema_registry.get(), std::move(async_policy_loader));
  policy_provider->Init(schema_registry.get());

  policy::PolicyServiceImpl::Providers providers;
  providers.push_back(policy_provider.get());
  std::unique_ptr<policy::PolicyServiceImpl> policy_service =
      std::make_unique<policy::PolicyServiceImpl>(std::move(providers));

  policy::PolicyService* borrowed_policy_service = policy_service.get();
  return base::WrapUnique(new PolicyWatcher(
      borrowed_policy_service, std::move(policy_service),
      std::move(policy_provider), std::move(schema_registry)));
}

std::unique_ptr<PolicyWatcher> PolicyWatcher::CreateWithPolicyService(
    policy::PolicyService* policy_service) {
  DCHECK(policy_service);
  return base::WrapUnique(new PolicyWatcher(policy_service, nullptr, nullptr,
                                            CreateSchemaRegistry()));
}

#if BUILDFLAG(IS_WIN)
void PolicyWatcher::WatchForRegistryChanges() {
  if (!policy_key_.Valid()) {
    auto open_result =
        policy_key_.Open(HKEY_LOCAL_MACHINE, kChromePolicyKey, KEY_NOTIFY);
    if (open_result != ERROR_SUCCESS) {
      LOG(WARNING) << "Failed to open Chrome policy registry key due to error: "
                   << open_result;
      return;
    }
  }

  // base::Unretained is sound as |policy_key_| is destroyed before we start
  // tearing down the various policy service members. Once the PolicyService has
  // finished refreshing the policy list, we need to set up our watcher again as
  // it only fires once.
  auto watch_result = policy_key_.StartWatching(
      base::BindOnce(&policy::PolicyService::RefreshPolicies,
                     base::Unretained(policy_service_),
                     base::BindOnce(&PolicyWatcher::WatchForRegistryChanges,
                                    base::Unretained(this)),
                     policy::PolicyFetchReason::kCrdHostPolicyWatcher));
  if (!watch_result) {
    LOG(WARNING) << "Failed to register for Chrome policy registry key changes";
    policy_key_.Close();
  }
}
#endif

std::unique_ptr<PolicyWatcher> PolicyWatcher::CreateWithTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner,
    policy::ManagementService* management_service) {
  // Create platform-specific PolicyLoader. Always read the Chrome policies
  // (even on Chromium) so that policy enforcement can't be bypassed by running
  // Chromium.
  std::unique_ptr<policy::AsyncPolicyLoader> policy_loader;
#if BUILDFLAG(IS_WIN)
  policy_loader = std::make_unique<policy::PolicyLoaderWin>(
      file_task_runner, management_service, kChromePolicyKey);
#elif BUILDFLAG(IS_APPLE)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Explicitly watch the "com.google.Chrome" bundle ID, no matter what this
  // app's bundle ID actually is. All channels of Chrome should obey the same
  // policies.
  CFStringRef bundle_id = CFSTR("com.google.Chrome");
#else
  base::apple::ScopedCFTypeRef<CFStringRef> bundle_id_scoper =
      base::SysUTF8ToCFStringRef(base::apple::BaseBundleID());
  CFStringRef bundle_id = bundle_id_scoper.get();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  policy_loader = std::make_unique<policy::PolicyLoaderMac>(
      file_task_runner,
      policy::PolicyLoaderMac::GetManagedPolicyPath(bundle_id),
      std::make_unique<MacPreferences>(), bundle_id);
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  policy_loader = std::make_unique<policy::ConfigDirPolicyLoader>(
      file_task_runner, base::FilePath(policy::kPolicyPath),
      policy::POLICY_SCOPE_MACHINE);
#elif BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
  policy::PolicyServiceImpl::Providers providers;
  std::unique_ptr<policy::PolicyService> owned_policy_service(
      new policy::PolicyServiceImpl(providers));
  return base::WrapUnique(new PolicyWatcher(owned_policy_service.get(),
                                            std::move(owned_policy_service),
                                            nullptr, CreateSchemaRegistry()));
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  NOTREACHED() << "CreateWithPolicyService() should be used on ChromeOS.";
#else
#error OS that is not yet supported by PolicyWatcher code.
#endif

  return PolicyWatcher::CreateFromPolicyLoader(std::move(policy_loader));
}

std::unique_ptr<PolicyWatcher> PolicyWatcher::CreateFromPolicyLoaderForTesting(
    std::unique_ptr<policy::AsyncPolicyLoader> async_policy_loader) {
  return CreateFromPolicyLoader(std::move(async_policy_loader));
}

}  // namespace remoting
