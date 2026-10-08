// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RESOURCE_BROKER_RESOURCE_BROKER_HOST_H_
#define CONTENT_BROWSER_RESOURCE_BROKER_RESOURCE_BROKER_HOST_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "content/services/resource_broker/public/mojom/resource_broker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// Browser-side host controller for the Resource Broker utility service.
// Manages service process lifetime via ServiceProcessHost, handles relaunch
// cooldowns (10s), and mitigates rapid crash loops (permanently disabling the
// service for the session if it crashes 3+ times in 10 minutes).
//
// Thread affinity: All methods must be invoked on the UI thread
// (BrowserThread::UI) in production. A sequence checker is intentionally used
// internally rather than BrowserThread::UI checks to allow unit tests to run
// with a lightweight base::test::TaskEnvironment without
// BrowserTaskEnvironment.
class CONTENT_EXPORT ResourceBrokerHost {
 public:
  // Discrete lifecycle events logged to ResourceBroker.LifecycleEvent.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(ResourceBrokerLifecycleEvent)
  enum class ResourceBrokerLifecycleEvent {
    kLaunched = 0,
    kDisabledDueToCrashLoop = 1,
    kMaxValue = kDisabledDueToCrashLoop,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/resource_broker/enums.xml:ResourceBrokerLifecycleEvent)

  using ServiceLauncherForTesting = base::RepeatingCallback<void(
      mojo::PendingReceiver<resource_broker::mojom::ResourceBrokerService>)>;

  static ResourceBrokerHost& GetInstance();

  ResourceBrokerHost(const ResourceBrokerHost&) = delete;
  ResourceBrokerHost& operator=(const ResourceBrokerHost&) = delete;

  ~ResourceBrokerHost();

  // Returns the active interface pointer to the Resource Broker Service, or
  // nullptr if the service is disabled due to a crash loop or during the
  // relaunch cooldown period. Launches the service lazily if not currently
  // running.
  //
  // NOTE: Callers must not cache the returned pointer across asynchronous
  // yields, as the underlying service may disconnect or be reset.
  resource_broker::mojom::ResourceBrokerService* GetService();

  // Returns the session nonce minted at construction; stable across broker
  // relaunches within a session.
  base::UnguessableToken GetSessionNonce() const;

  // Reset singleton state for testing.
  static void ResetForTesting();

  // Flushes the service remote for testing. No-op if the service is not bound,
  // e.g. after a disconnect has already been handled.
  void MaybeFlushForTesting();

  // Side-effect-free inspector to check whether the service remote is bound.
  bool IsServiceRunningForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return service_remote_.is_bound();
  }

  // Injects a custom service launcher for testing GetService() and relaunch
  // behavior without spawning a child process.
  void SetServiceLauncherForTesting(ServiceLauncherForTesting launcher);

  // Helper to create and validate the broker configuration.
  static resource_broker::mojom::BrokerConfigPtr CreateConfig(
      const base::UnguessableToken& session_nonce);

 private:
  ResourceBrokerHost();

  void LaunchService();
  void OnServiceDisconnected();

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Remote<resource_broker::mojom::ResourceBrokerService> service_remote_;
  ServiceLauncherForTesting service_launcher_for_testing_;
  base::UnguessableToken session_nonce_ = base::UnguessableToken::Create();
  bool disabled_for_session_ = false;
  bool initial_launch_recorded_ = false;

  base::TimeTicks last_disconnect_time_;

  std::vector<base::TimeTicks> crash_timestamps_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RESOURCE_BROKER_RESOURCE_BROKER_HOST_H_
