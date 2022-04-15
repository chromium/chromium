// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most of this code is copied from:
//   src/chrome/browser/policy/asynchronous_policy_loader.{h,cc}

#include "remoting/host/policy_watcher.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/async_policy_loader.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"
#include "remoting/host/third_party_auth_config.h"
#include "remoting/protocol/port_range.h"

#if !defined(NDEBUG)
#include "base/json/json_reader.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "components/policy/core/common/policy_loader_win.h"
#elif BUILDFLAG(IS_APPLE)
#include "components/policy/core/common/policy_loader_mac.h"
#include "components/policy/core/common/preferences_mac.h"
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
#include "components/policy/core/common/config_dir_policy_loader.h"
#endif

namespace remoting {

namespace key = ::policy::key;

namespace {

#if BUILDFLAG(IS_WIN)
constexpr wchar_t kChromePolicyKey[] = L"SOFTWARE\\Policies\\Google\\Chrome";
#endif

// Copies all policy values from one dictionary to another, using values from
// |default_values| if they are not set in |from|.
std::unique_ptr<base::DictionaryValue> CopyValuesAndAddDefaults(
    const base::DictionaryValue& from,
    const base::DictionaryValue& default_values) {
  std::unique_ptr<base::DictionaryValue> to(default_values.CreateDeepCopy());
  for (base::DictionaryValue::Iterator i(default_values); !i.IsAtEnd();
       i.Advance()) {
    const base::Value* value = nullptr;

    // If the policy isn't in |from|, use the default.
    if (!from.Get(i.key(), &value)) {
      continue;
    }

    CHECK(value->type() == i.value().type());
    to->Set(i.key(), base::Value::ToUniquePtrValue(value->Clone()));
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

  std::unique_ptr<policy::SchemaRegistry> schema_registry(
      new policy::SchemaRegistry());
  schema_registry->RegisterComponent(GetPolicyNamespace(), schema);
  return schema_registry;
}

std::unique_ptr<base::DictionaryValue> CopyChromotingPoliciesIntoDictionary(
    const policy::PolicyMap& current) {
  const char kPolicyNameSubstring[] = "RemoteAccessHost";
  std::unique_ptr<base::DictionaryValue> policy_dict(
      new base::DictionaryValue());
  for (const auto& entry : current) {
    const std::string& key = entry.first;
    // |value_unsafe| is used due to multiple policy types being handled.
    const base::Value* value = entry.second.value_unsafe();

    // Copying only Chromoting-specific policies helps avoid false alarms
    // raised by NormalizePolicies below (such alarms shutdown the host).
    // TODO(lukasza): Removing this somewhat brittle filtering will be possible
    //                after having separate, Chromoting-specific schema.
    if (key.find(kPolicyNameSubstring) != std::string::npos) {
      policy_dict->Set(key, base::Value::ToUniquePtrValue(value->Clone()));
    }
  }

  return policy_dict;
}

// Takes a dictionary containing only 1) recognized policy names and 2)
// well-typed policy values and further verifies policy contents.
bool VerifyWellformedness(const base::DictionaryValue& changed_policies) {
  // Verify ThirdPartyAuthConfig policy.
  ThirdPartyAuthConfig not_used;
  switch (ThirdPartyAuthConfig::Parse(changed_policies, &not_used)) {
    case ThirdPartyAuthConfig::NoPolicy:
    case ThirdPartyAuthConfig::ParsingSuccess:
      break;  // Well-formed.
    case ThirdPartyAuthConfig::InvalidPolicy:
      return false;  // Malformed.
    default:
      NOTREACHED();
      return false;
  }

  // Verify UdpPortRange policy.
  std::string udp_port_range_string;
  PortRange udp_port_range;
  if (changed_policies.GetString(policy::key::kRemoteAccessHostUdpPortRange,
                                 &udp_port_range_string)) {
    if (!PortRange::Parse(udp_port_range_string, &udp_port_range)) {
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

std::unique_ptr<base::DictionaryValue> PolicyWatcher::GetEffectivePolicies() {
  return effective_policies_->CreateDeepCopy();
}

std::unique_ptr<base::DictionaryValue> PolicyWatcher::GetPlatformPolicies() {
  return platform_policies_->CreateDeepCopy();
}

std::unique_ptr<base::DictionaryValue> PolicyWatcher::GetDefaultPolicies() {
  auto result = std::make_unique<base::DictionaryValue>();
  result->SetBoolKey(key::kRemoteAccessHostFirewallTraversal, true);
  result->SetBoolKey(key::kRemoteAccessHostRequireCurtain, false);
  result->SetBoolKey(key::kRemoteAccessHostMatchUsername, false);
  result->Set(key::kRemoteAccessHostClientDomainList,
              std::make_unique<base::ListValue>());
  result->Set(key::kRemoteAccessHostDomainList,
              std::make_unique<base::ListValue>());
  result->SetStringKey(key::kRemoteAccessHostTokenUrl, std::string());
  result->SetStringKey(key::kRemoteAccessHostTokenValidationUrl, std::string());
  result->SetStringKey(key::kRemoteAccessHostTokenValidationCertificateIssuer,
                       std::string());
  result->SetBoolKey(key::kRemoteAccessHostAllowClientPairing, true);
  result->SetBoolKey(key::kRemoteAccessHostAllowGnubbyAuth, true);
  result->SetBoolKey(key::kRemoteAccessHostAllowRelayedConnection, true);
  result->SetStringKey(key::kRemoteAccessHostUdpPortRange, "");
  result->SetBoolKey(key::kRemoteAccessHostAllowUiAccessForRemoteAssistance,
                     false);
  result->SetIntKey(key::kRemoteAccessHostClipboardSizeBytes, -1);
  result->SetBoolKey(key::kRemoteAccessHostAllowRemoteSupportConnections, true);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  result->SetBoolKey(key::kRemoteAccessHostAllowFileTransfer, true);
  result->SetBoolKey(key::kRemoteAccessHostEnableUserInterface, true);
  result->SetBoolKey(key::kRemoteAccessHostAllowRemoteAccessConnections, true);
  result->SetIntKey(key::kRemoteAccessHostMaximumSessionDurationMinutes, 0);
#endif
  return result;
}

void PolicyWatcher::SignalPolicyError() {
  effective_policies_->DictClear();
  platform_policies_->DictClear();
  policy_error_callback_.Run();
}

PolicyWatcher::PolicyWatcher(
    policy::PolicyService* policy_service,
    std::unique_ptr<policy::PolicyService> owned_policy_service,
    std::unique_ptr<policy::ConfigurationPolicyProvider> owned_policy_provider,
    std::unique_ptr<policy::SchemaRegistry> owned_schema_registry)
    : effective_policies_(new base::DictionaryValue()),
      platform_policies_(new base::DictionaryValue()),
      default_values_(GetDefaultPolicies()),
      policy_service_(policy_service),
      owned_schema_registry_(std::move(owned_schema_registry)),
      owned_policy_provider_(std::move(owned_policy_provider)),
      owned_policy_service_(std::move(owned_policy_service)) {
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

bool PolicyWatcher::NormalizePolicies(base::DictionaryValue* policy_dict) {
  // Allowing unrecognized policy names allows presence of
  // 1) comments (i.e. JSON of the form: { "_comment": "blah", ... }),
  // 2) policies intended for future/newer versions of the host,
  // 3) policies not supported on all OS-s (i.e. RemoteAccessHostMatchUsername
  //    is not supported on Windows and therefore policy_templates.json omits
  //    schema for this policy on this particular platform).
  auto strategy = policy::SCHEMA_ALLOW_UNKNOWN;

  std::string path;
  std::string error;
  bool changed = false;
  const policy::Schema* schema = GetPolicySchema();
  if (schema->Normalize(policy_dict, strategy, &path, &error, &changed)) {
    if (changed) {
      LOG(WARNING) << "Unknown (unrecognized or unsupported) policy: " << path
                   << ": " << error;
    }
    HandleDeprecatedPolicies(policy_dict);
    return true;
  } else {
    LOG(ERROR) << "Invalid policy contents: " << path << ": " << error;
    return false;
  }
}

void PolicyWatcher::HandleDeprecatedPolicies(base::DictionaryValue* dict) {
  // RemoteAccessHostDomain
  if (dict->FindKey(policy::key::kRemoteAccessHostDomain)) {
    if (!dict->FindKey(policy::key::kRemoteAccessHostDomainList)) {
      std::string domain;
      dict->GetString(policy::key::kRemoteAccessHostDomain, &domain);
      if (!domain.empty()) {
        auto list = std::make_unique<base::ListValue>();
        list->Append(domain);
        dict->Set(policy::key::kRemoteAccessHostDomainList, std::move(list));
      }
    }
    dict->RemoveKey(policy::key::kRemoteAccessHostDomain);
  }

  // RemoteAccessHostClientDomain
  if (const std::string* domain =
          dict->FindStringKey(policy::key::kRemoteAccessHostClientDomain)) {
    if (!dict->FindKey(policy::key::kRemoteAccessHostClientDomainList)) {
      if (!domain->empty()) {
        auto list = std::make_unique<base::ListValue>();
        list->Append(*domain);
        dict->Set(policy::key::kRemoteAccessHostClientDomainList,
                  std::move(list));
      }
    }
    dict->RemoveKey(policy::key::kRemoteAccessHostClientDomain);
  }
}

namespace {
void CopyDictionaryValue(const base::DictionaryValue& from,
                         base::DictionaryValue& to,
                         std::string key) {
  const base::Value* value;
  if (from.Get(key, &value)) {
    to.Set(key, base::Value::ToUniquePtrValue(value->Clone()));
  }
}
}  // namespace

std::unique_ptr<base::DictionaryValue>
PolicyWatcher::StoreNewAndReturnChangedPolicies(
    std::unique_ptr<base::DictionaryValue> new_policies) {
  // Find the changed policies.
  std::unique_ptr<base::DictionaryValue> changed_policies(
      new base::DictionaryValue());
  base::DictionaryValue::Iterator iter(*new_policies);
  while (!iter.IsAtEnd()) {
    base::Value* old_policy;
    if (!(effective_policies_->Get(iter.key(), &old_policy) &&
          *old_policy == iter.value())) {
      changed_policies->Set(
          iter.key(), base::Value::ToUniquePtrValue(iter.value().Clone()));
    }
    iter.Advance();
  }

  // If one of ThirdPartyAuthConfig policies changed, we need to include all.
  if (changed_policies->FindKey(key::kRemoteAccessHostTokenUrl) ||
      changed_policies->FindKey(key::kRemoteAccessHostTokenValidationUrl) ||
      changed_policies->FindKey(
          key::kRemoteAccessHostTokenValidationCertificateIssuer)) {
    CopyDictionaryValue(*new_policies, *changed_policies,
                        key::kRemoteAccessHostTokenUrl);
    CopyDictionaryValue(*new_policies, *changed_policies,
                        key::kRemoteAccessHostTokenValidationUrl);
    CopyDictionaryValue(*new_policies, *changed_policies,
                        key::kRemoteAccessHostTokenValidationCertificateIssuer);
  }

  // Save the new policies.
  effective_policies_.swap(new_policies);

  return changed_policies;
}

void PolicyWatcher::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                    const policy::PolicyMap& previous,
                                    const policy::PolicyMap& current) {
  std::unique_ptr<base::DictionaryValue> new_policies =
      CopyChromotingPoliciesIntoDictionary(current);

  // Check for mistyped values and get rid of unknown policies.
  if (!NormalizePolicies(new_policies.get())) {
    SignalPolicyError();
    return;
  }

  platform_policies_ = new_policies->CreateDeepCopy();

  // Use default values for any missing policies.
  std::unique_ptr<base::DictionaryValue> filled_policies =
      CopyValuesAndAddDefaults(*new_policies, *default_values_);

  // Limit reporting to only the policies that were changed.
  std::unique_ptr<base::DictionaryValue> changed_policies =
      StoreNewAndReturnChangedPolicies(std::move(filled_policies));
  if (changed_policies->DictEmpty()) {
    return;
  }

  // Verify that we are calling the callback with valid policies.
  if (!VerifyWellformedness(*changed_policies)) {
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
  std::unique_ptr<policy::AsyncPolicyProvider> policy_provider(
      new policy::AsyncPolicyProvider(schema_registry.get(),
                                      std::move(async_policy_loader)));
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
                                    base::Unretained(this))));
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
      file_task_runner, management_service, kChromePolicyKey,
      true /* is_dev_registry_key_supported */);
#elif BUILDFLAG(IS_APPLE)
  CFStringRef bundle_id = CFSTR("com.google.Chrome");
  policy_loader = std::make_unique<policy::PolicyLoaderMac>(
      file_task_runner,
      policy::PolicyLoaderMac::GetManagedPolicyPath(bundle_id),
      new MacPreferences(), bundle_id);
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  policy_loader = std::make_unique<policy::ConfigDirPolicyLoader>(
      file_task_runner,
      base::FilePath(FILE_PATH_LITERAL("/etc/opt/chrome/policies")),
      policy::POLICY_SCOPE_MACHINE);
#elif BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
  policy::PolicyServiceImpl::Providers providers;
  std::unique_ptr<policy::PolicyService> owned_policy_service(
      new policy::PolicyServiceImpl(providers));
  return base::WrapUnique(new PolicyWatcher(
      owned_policy_service.get(), std::move(owned_policy_service), nullptr,
      CreateSchemaRegistry()));
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  NOTREACHED() << "CreateWithPolicyService() should be used on ChromeOS.";
  return nullptr;
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
