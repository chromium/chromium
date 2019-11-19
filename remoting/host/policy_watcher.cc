// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most of this code is copied from:
//   src/chrome/browser/policy/asynchronous_policy_loader.{h,cc}

#include "remoting/host/policy_watcher.h"

#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
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

#if defined(OS_WIN)
#include "components/policy/core/common/policy_loader_win.h"
#elif defined(OS_MACOSX)
#include "components/policy/core/common/policy_loader_mac.h"
#include "components/policy/core/common/preferences_mac.h"
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
#include "components/policy/core/common/config_dir_policy_loader.h"
#endif

namespace remoting {

namespace key = ::policy::key;

namespace {

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
    to->Set(i.key(), value->CreateDeepCopy());
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
    const base::Value* value = entry.second.value.get();

    // Copying only Chromoting-specific policies helps avoid false alarms
    // raised by NormalizePolicies below (such alarms shutdown the host).
    // TODO(lukasza): Removing this somewhat brittle filtering will be possible
    //                after having separate, Chromoting-specific schema.
    if (key.find(kPolicyNameSubstring) != std::string::npos) {
      policy_dict->Set(key, value->CreateDeepCopy());
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

std::unique_ptr<base::DictionaryValue> PolicyWatcher::GetCurrentPolicies() {
  return old_policies_->CreateDeepCopy();
}

std::unique_ptr<base::DictionaryValue> PolicyWatcher::GetDefaultPolicies() {
  auto result = std::make_unique<base::DictionaryValue>();
  result->SetBoolean(key::kRemoteAccessHostFirewallTraversal, true);
  result->SetBoolean(key::kRemoteAccessHostRequireCurtain, false);
  result->SetBoolean(key::kRemoteAccessHostMatchUsername, false);
  result->Set(key::kRemoteAccessHostClientDomainList,
              std::make_unique<base::ListValue>());
  result->Set(key::kRemoteAccessHostDomainList,
              std::make_unique<base::ListValue>());
  // TODO(yuweih): kRemoteAccessHostTalkGadgetPrefix is not used any more. Clean
  // this up.
  result->SetString(key::kRemoteAccessHostTalkGadgetPrefix, std::string());
  result->SetString(key::kRemoteAccessHostTokenUrl, std::string());
  result->SetString(key::kRemoteAccessHostTokenValidationUrl, std::string());
  result->SetString(key::kRemoteAccessHostTokenValidationCertificateIssuer,
                    std::string());
  result->SetBoolean(key::kRemoteAccessHostAllowClientPairing, true);
  result->SetBoolean(key::kRemoteAccessHostAllowGnubbyAuth, true);
  result->SetBoolean(key::kRemoteAccessHostAllowRelayedConnection, true);
  result->SetString(key::kRemoteAccessHostUdpPortRange, "");
  result->SetBoolean(key::kRemoteAccessHostAllowUiAccessForRemoteAssistance,
                     false);
#if !defined(OS_CHROMEOS)
  result->SetBoolean(key::kRemoteAccessHostAllowFileTransfer, true);
#endif
  return result;
}

void PolicyWatcher::SignalPolicyError() {
  old_policies_->Clear();
  policy_error_callback_.Run();
}

PolicyWatcher::PolicyWatcher(
    policy::PolicyService* policy_service,
    std::unique_ptr<policy::PolicyService> owned_policy_service,
    std::unique_ptr<policy::ConfigurationPolicyProvider> owned_policy_provider,
    std::unique_ptr<policy::SchemaRegistry> owned_schema_registry)
    : old_policies_(new base::DictionaryValue()),
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
  if (dict->HasKey(policy::key::kRemoteAccessHostDomain)) {
    if (!dict->HasKey(policy::key::kRemoteAccessHostDomainList)) {
      std::string domain;
      dict->GetString(policy::key::kRemoteAccessHostDomain, &domain);
      if (!domain.empty()) {
        auto list = std::make_unique<base::ListValue>();
        list->AppendString(domain);
        dict->Set(policy::key::kRemoteAccessHostDomainList, std::move(list));
      }
    }
    dict->Remove(policy::key::kRemoteAccessHostDomain, nullptr);
  }

  // RemoteAccessHostClientDomain
  if (dict->HasKey(policy::key::kRemoteAccessHostClientDomain)) {
    if (!dict->HasKey(policy::key::kRemoteAccessHostClientDomainList)) {
      std::string domain;
      dict->GetString(policy::key::kRemoteAccessHostClientDomain, &domain);
      if (!domain.empty()) {
        auto list = std::make_unique<base::ListValue>();
        list->AppendString(domain);
        dict->Set(policy::key::kRemoteAccessHostClientDomainList,
                  std::move(list));
      }
    }
    dict->Remove(policy::key::kRemoteAccessHostClientDomain, nullptr);
  }
}

namespace {
void CopyDictionaryValue(const base::DictionaryValue& from,
                         base::DictionaryValue& to,
                         std::string key) {
  const base::Value* value;
  if (from.Get(key, &value)) {
    to.Set(key, value->CreateDeepCopy());
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
    if (!(old_policies_->Get(iter.key(), &old_policy) &&
          old_policy->Equals(&iter.value()))) {
      changed_policies->Set(iter.key(), iter.value().CreateDeepCopy());
    }
    iter.Advance();
  }

  // If one of ThirdPartyAuthConfig policies changed, we need to include all.
  if (changed_policies->HasKey(key::kRemoteAccessHostTokenUrl) ||
      changed_policies->HasKey(key::kRemoteAccessHostTokenValidationUrl) ||
      changed_policies->HasKey(
          key::kRemoteAccessHostTokenValidationCertificateIssuer)) {
    CopyDictionaryValue(*new_policies, *changed_policies,
                        key::kRemoteAccessHostTokenUrl);
    CopyDictionaryValue(*new_policies, *changed_policies,
                        key::kRemoteAccessHostTokenValidationUrl);
    CopyDictionaryValue(*new_policies, *changed_policies,
                        key::kRemoteAccessHostTokenValidationCertificateIssuer);
  }

  // Save the new policies.
  old_policies_.swap(new_policies);

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

  // Use default values for any missing policies.
  std::unique_ptr<base::DictionaryValue> filled_policies =
      CopyValuesAndAddDefaults(*new_policies, *default_values_);

  // Limit reporting to only the policies that were changed.
  std::unique_ptr<base::DictionaryValue> changed_policies =
      StoreNewAndReturnChangedPolicies(std::move(filled_policies));
  if (changed_policies->empty()) {
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

std::unique_ptr<PolicyWatcher> PolicyWatcher::CreateWithTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner) {
  // Create platform-specific PolicyLoader. Always read the Chrome policies
  // (even on Chromium) so that policy enforcement can't be bypassed by running
  // Chromium.
  std::unique_ptr<policy::AsyncPolicyLoader> policy_loader;
#if defined(OS_WIN)
  policy_loader.reset(new policy::PolicyLoaderWin(
      file_task_runner, L"SOFTWARE\\Policies\\Google\\Chrome"));
#elif defined(OS_MACOSX)
  CFStringRef bundle_id = CFSTR("com.google.Chrome");
  policy_loader.reset(new policy::PolicyLoaderMac(
      file_task_runner,
      policy::PolicyLoaderMac::GetManagedPolicyPath(bundle_id),
      new MacPreferences(), bundle_id));
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
  policy_loader.reset(new policy::ConfigDirPolicyLoader(
      file_task_runner,
      base::FilePath(FILE_PATH_LITERAL("/etc/opt/chrome/policies")),
      policy::POLICY_SCOPE_MACHINE));
#elif defined(OS_ANDROID)
  NOTIMPLEMENTED();
  policy::PolicyServiceImpl::Providers providers;
  std::unique_ptr<policy::PolicyService> owned_policy_service(
      new policy::PolicyServiceImpl(providers));
  return base::WrapUnique(new PolicyWatcher(
      owned_policy_service.get(), std::move(owned_policy_service), nullptr,
      CreateSchemaRegistry()));
#elif defined(OS_CHROMEOS)
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
