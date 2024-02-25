// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_POLICY_WATCHER_H_
#define REMOTING_HOST_POLICY_WATCHER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_service.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/registry.h"
#endif

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace policy {
class AsyncPolicyLoader;
class ConfigurationPolicyProvider;
class ManagementService;
class Schema;
class SchemaRegistry;
}  // namespace policy

namespace remoting {

// Watches for changes to the managed remote access host policies.
class PolicyWatcher : public policy::PolicyService::Observer {
 public:
  // Called first with all policies, and subsequently with any changed policies.
  // Policies that are unchanged will be absent in the returned dictionary.
  // If a policy has no default value but is unset, it will be an empty Value,
  // i.e., of type NONE.
  using PolicyUpdatedCallback =
      base::RepeatingCallback<void(base::Value::Dict)>;

  // Called after detecting malformed policies.
  using PolicyErrorCallback = base::RepeatingCallback<void()>;

  PolicyWatcher(const PolicyWatcher&) = delete;
  PolicyWatcher& operator=(const PolicyWatcher&) = delete;

  ~PolicyWatcher() override;

  // This guarantees that the |policy_updated_callback| is called at least once
  // with the current policies.  After that, |policy_updated_callback| will be
  // called whenever a change to any policy is detected. It will then be called
  // only with the changed policies.
  //
  // |policy_error_callback| will be called when malformed policies are detected
  // (i.e. wrong type of policy value, or unparseable files under
  // $POLICY_PATH/managed).
  // When called, the |policy_error_callback| is responsible for mitigating the
  // security risk of running with incorrectly formulated policies (by either
  // shutting down or locking down the host).
  // After calling |policy_error_callback| PolicyWatcher will continue watching
  // for policy changes and will call |policy_updated_callback| when the error
  // is recovered from and may call |policy_error_callback| when new errors are
  // found.
  virtual void StartWatching(
      const PolicyUpdatedCallback& policy_updated_callback,
      const PolicyErrorCallback& policy_error_callback);

  // Return the current policies. If the policies have not yet been read, or if
  // an error occurred, the returned dictionary will be empty.  The dictionary
  // returned is the union of |platform_policies_| and |default_values_|.
  base::Value::Dict GetEffectivePolicies();

  // Return the set of policies which have been explicitly set on the machine.
  // If the policies have not yet been read, no policies have been set, or if
  // an error occurred, the returned dictionary will be empty.
  base::Value::Dict GetPlatformPolicies();

  // Return the default policy values.
  static base::Value::Dict GetDefaultPolicies();

  // Specify a |policy_service| to borrow (on Chrome OS, from the browser
  // process). PolicyWatcher must be used on the thread on which it is created.
  // |policy_service| is called on the same thread.
  //
  // When |policy_service| is specified then BrowserThread::UI is used for
  // PolicyUpdatedCallback and PolicyErrorCallback.
  static std::unique_ptr<PolicyWatcher> CreateWithPolicyService(
      policy::PolicyService* policy_service);

  // Construct and a new PolicyService for non-ChromeOS platforms.
  // PolicyWatcher must be used on the thread on which it is created.
  //
  // |file_task_runner| is used for reading the policy from files / registry /
  // preferences (which are blocking operations). |file_task_runner| should be
  // of TYPE_IO type.
  static std::unique_ptr<PolicyWatcher> CreateWithTaskRunner(
      const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner,
      policy::ManagementService* management_service);

  // Creates a PolicyWatcher from the given loader instead of loading the policy
  // from the default location.
  //
  // This can be used with FakeAsyncPolicyLoader to test policy handling of
  // other components.
  static std::unique_ptr<PolicyWatcher> CreateFromPolicyLoaderForTesting(
      std::unique_ptr<policy::AsyncPolicyLoader> async_policy_loader);

 private:
  friend class PolicyWatcherTest;

  // Gets Chromoting schema stored inside |owned_schema_registry_|.
  const policy::Schema* GetPolicySchema() const;

  // Normalizes policies using Schema::Normalize and converts deprecated
  // policies.
  //
  // - Returns false if |dict| is invalid, e.g. contains mistyped policy values.
  // - Returns true if |dict| was valid or got normalized.
  bool NormalizePolicies(base::Value* dict);

  // Converts each deprecated policy to its replacement if and only if the
  // replacement policy is not set, and removes deprecated policied from dict.
  void HandleDeprecatedPolicies(base::Value::Dict* dict);

  // Stores |new_policies| into |effective_policies_|.  Returns dictionary with
  // items from |new_policies| that are different from the old
  // |effective_policies_|.
  base::Value::Dict StoreNewAndReturnChangedPolicies(
      base::Value::Dict new_policies);

  // Signals policy error to the registered |PolicyErrorCallback|.
  void SignalPolicyError();

  // |policy_service_task_runner| is the task runner where it is safe
  // to call |policy_service_| methods and where we expect to get callbacks
  // from |policy_service_|.
  PolicyWatcher(policy::PolicyService* policy_service,
                std::unique_ptr<policy::PolicyService> owned_policy_service,
                std::unique_ptr<policy::ConfigurationPolicyProvider>
                    owned_policy_provider,
                std::unique_ptr<policy::SchemaRegistry> owned_schema_registry);

  // Creates PolicyWatcher that wraps the owned |async_policy_loader| with an
  // appropriate PolicySchema.
  //
  // |policy_service_task_runner| is passed through to the constructor of
  // PolicyWatcher.
  static std::unique_ptr<PolicyWatcher> CreateFromPolicyLoader(
      std::unique_ptr<policy::AsyncPolicyLoader> async_policy_loader);

  // PolicyService::Observer interface.
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;
  void OnPolicyServiceInitialized(policy::PolicyDomain domain) override;

#if BUILDFLAG(IS_WIN)
  void WatchForRegistryChanges();
#endif

  PolicyUpdatedCallback policy_updated_callback_;
  PolicyErrorCallback policy_error_callback_;

  // The combined set of policies (|platform_policies_| + |default_values_|)
  // which define the effective policy set.
  base::Value::Dict effective_policies_;

  // The policies which have had their values explicitly set via a policy entry.
  base::Value::Dict platform_policies_;

  // The set of policy values to use if a policy has not been explicitly set.
  base::Value::Dict default_values_;

  // Order of fields below is important to ensure destruction takes object
  // dependencies into account:
  // - |policy_service_| may be |owned_policy_service_|.
  // - |owned_policy_service_| uses |owned_policy_provider_|
  // - |owned_policy_provider_| uses |owned_schema_registry_|
  std::unique_ptr<policy::SchemaRegistry> owned_schema_registry_;
  std::unique_ptr<policy::ConfigurationPolicyProvider> owned_policy_provider_;
  std::unique_ptr<policy::PolicyService> owned_policy_service_;
  raw_ptr<policy::PolicyService> policy_service_;

#if BUILDFLAG(IS_WIN)
  // |policy_key_| relies on |policy_service_| to notify the host of policy
  // changes. Make sure |policy_key_| is destroyed to prevent any notifications
  // from firing while the above objects are being torn down.
  base::win::RegKey policy_key_;
#endif

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_POLICY_WATCHER_H_
