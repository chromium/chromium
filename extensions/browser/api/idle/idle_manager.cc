// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/idle/idle_manager.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/idle/idle_api_constants.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/idle.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/power/power_policy_controller.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace keys = extensions::idle_api_constants;
namespace idle = extensions::api::idle;

namespace extensions {

namespace {

const int kDefaultIdleThreshold = 60;
const int kPollInterval = 1;

class DefaultEventDelegate : public IdleManager::EventDelegate {
 public:
  explicit DefaultEventDelegate(content::BrowserContext* context);
  ~DefaultEventDelegate() override;

  void OnStateChanged(const ExtensionId& extension_id,
                      ui::IdleState new_state) override;
  void RegisterObserver(EventRouter::Observer* observer) override;
  void UnregisterObserver(EventRouter::Observer* observer) override;

 private:
  const raw_ptr<content::BrowserContext> context_;
};

DefaultEventDelegate::DefaultEventDelegate(content::BrowserContext* context)
    : context_(context) {
}

DefaultEventDelegate::~DefaultEventDelegate() {
}

void DefaultEventDelegate::OnStateChanged(const ExtensionId& extension_id,
                                          ui::IdleState new_state) {
  base::Value::List args;
  args.Append(IdleManager::CreateIdleValue(new_state));
  auto event = std::make_unique<Event>(events::IDLE_ON_STATE_CHANGED,
                                       idle::OnStateChanged::kEventName,
                                       std::move(args), context_);
  EventRouter::Get(context_)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

void DefaultEventDelegate::RegisterObserver(EventRouter::Observer* observer) {
  EventRouter::Get(context_)
      ->RegisterObserver(observer, idle::OnStateChanged::kEventName);
}

void DefaultEventDelegate::UnregisterObserver(EventRouter::Observer* observer) {
  EventRouter::Get(context_)->UnregisterObserver(observer);
}

class DefaultIdleProvider : public IdleManager::IdleTimeProvider {
 public:
  DefaultIdleProvider();
  ~DefaultIdleProvider() override;

  ui::IdleState CalculateIdleState(int idle_threshold) override;
  int CalculateIdleTime() override;
  bool CheckIdleStateIsLocked() override;
};

DefaultIdleProvider::DefaultIdleProvider() {
}

DefaultIdleProvider::~DefaultIdleProvider() {
}

ui::IdleState DefaultIdleProvider::CalculateIdleState(int idle_threshold) {
  return ui::CalculateIdleState(idle_threshold);
}

int DefaultIdleProvider::CalculateIdleTime() {
  return ui::CalculateIdleTime();
}

bool DefaultIdleProvider::CheckIdleStateIsLocked() {
  return ui::CheckIdleStateIsLocked();
}

ui::IdleState IdleTimeToIdleState(bool locked,
                                  int idle_time,
                                  int idle_threshold) {
  ui::IdleState state;

  if (locked) {
    state = ui::IDLE_STATE_LOCKED;
  } else if (idle_time >= idle_threshold) {
    state = ui::IDLE_STATE_IDLE;
  } else {
    state = ui::IDLE_STATE_ACTIVE;
  }
  return state;
}

}  // namespace

IdleMonitor::IdleMonitor(ui::IdleState initial_state)
    : last_state(initial_state),
      listeners(0),
      threshold(kDefaultIdleThreshold) {
}

IdleManager::IdleManager(content::BrowserContext* context)
    : context_(context),
      last_state_(ui::IDLE_STATE_ACTIVE),
      idle_time_provider_(new DefaultIdleProvider()),
      event_delegate_(new DefaultEventDelegate(context)) {}

IdleManager::~IdleManager() {
}

void IdleManager::Init() {
  extension_registry_observation_.Observe(ExtensionRegistry::Get(context_));
  event_delegate_->RegisterObserver(this);
}

void IdleManager::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  event_delegate_->UnregisterObserver(this);
}

void IdleManager::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  monitors_.erase(extension->id());
}

void IdleManager::OnListenerAdded(const EventListenerInfo& details) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ++GetMonitor(details.extension_id)->listeners;
  StartPolling();
}

void IdleManager::OnListenerRemoved(const EventListenerInfo& details) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // During unload the monitor could already have been deleted. No need to do
  // anything in that case.
  auto it = monitors_.find(details.extension_id);
  if (it != monitors_.end()) {
    DCHECK_GT(it->second.listeners, 0);
    // Note: Deliberately leave the listener count as 0 rather than erase()ing
    // this record so that the threshold doesn't get reset when all listeners
    // are removed.
    --it->second.listeners;
  }
}

ui::IdleState IdleManager::QueryState(int threshold) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return idle_time_provider_->CalculateIdleState(threshold);
}

void IdleManager::SetThreshold(const ExtensionId& extension_id, int threshold) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GetMonitor(extension_id)->threshold = threshold;
}

int IdleManager::GetThresholdForTest(const ExtensionId& extension_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = monitors_.find(extension_id);

  return it == monitors_.end() ? kDefaultIdleThreshold : it->second.threshold;
}

base::TimeDelta IdleManager::GetAutoLockDelay() const {
  DCHECK(thread_checker_.CalledOnValidThread());
#if BUILDFLAG(IS_CHROMEOS)
  return chromeos::PowerPolicyController::Get()
      ->GetMaxPolicyAutoScreenLockDelay();
#else
  return base::TimeDelta();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

// static
base::Value IdleManager::CreateIdleValue(ui::IdleState idle_state) {
  const char* description;

  if (idle_state == ui::IDLE_STATE_ACTIVE) {
    description = keys::kStateActive;
  } else if (idle_state == ui::IDLE_STATE_IDLE) {
    description = keys::kStateIdle;
  } else {
    description = keys::kStateLocked;
  }

  return base::Value(description);
}

void IdleManager::SetEventDelegateForTest(
    std::unique_ptr<EventDelegate> event_delegate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  event_delegate_ = std::move(event_delegate);
}

void IdleManager::SetIdleTimeProviderForTest(
    std::unique_ptr<IdleTimeProvider> idle_time_provider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  idle_time_provider_ = std::move(idle_time_provider);
}

IdleMonitor* IdleManager::GetMonitor(const ExtensionId& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = monitors_.find(extension_id);

  if (it == monitors_.end()) {
    it = monitors_.insert(std::make_pair(extension_id,
                                         IdleMonitor(last_state_))).first;
  }
  return &it->second;
}

void IdleManager::StartPolling() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!poll_timer_.IsRunning()) {
    poll_timer_.Start(FROM_HERE, base::Seconds(kPollInterval), this,
                      &IdleManager::UpdateIdleState);
  }
}

void IdleManager::StopPolling() {
  DCHECK(thread_checker_.CalledOnValidThread());
  poll_timer_.Stop();
}

void IdleManager::UpdateIdleState() {
  DCHECK(thread_checker_.CalledOnValidThread());
  int idle_time = idle_time_provider_->CalculateIdleTime();
  bool locked = idle_time_provider_->CheckIdleStateIsLocked();
  int listener_count = 0;

  // Remember this state for initializing new event listeners.
  last_state_ = IdleTimeToIdleState(locked, idle_time, kDefaultIdleThreshold);

  for (auto it = monitors_.begin(); it != monitors_.end(); ++it) {
    IdleMonitor& monitor = it->second;
    ui::IdleState new_state =
        IdleTimeToIdleState(locked, idle_time, monitor.threshold);
    // TODO(kalman): Use EventRouter::HasListeners for these sorts of checks.
    if (monitor.listeners > 0 && monitor.last_state != new_state)
      event_delegate_->OnStateChanged(it->first, new_state);
    monitor.last_state = new_state;
    listener_count += monitor.listeners;
  }

  if (listener_count == 0)
    StopPolling();
}

}  // namespace extensions
