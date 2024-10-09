// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/message_tracker.h"

#include <map>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

namespace {

MessageTracker::TestObserver* g_test_observer = nullptr;

class MessageTrackerFactory : public BrowserContextKeyedServiceFactory {
 public:
  MessageTrackerFactory();
  MessageTrackerFactory(const MessageTrackerFactory&) = delete;
  MessageTrackerFactory& operator=(const MessageTrackerFactory&) = delete;
  ~MessageTrackerFactory() override = default;

  MessageTracker* GetForBrowserContext(content::BrowserContext* context);

 private:
  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

MessageTrackerFactory::MessageTrackerFactory()
    : BrowserContextKeyedServiceFactory(
          "MessageTracker",
          BrowserContextDependencyManager::GetInstance()) {}

MessageTracker* MessageTrackerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<MessageTracker*>(
      GetServiceForBrowserContext(browser_context, /*create=*/true));
}

content::BrowserContext* MessageTrackerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // One instance will exist across incognito and regular contexts.
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context, /*force_guest_profile=*/true);
}

std::unique_ptr<KeyedService>
MessageTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<MessageTracker>(context);
}

}  // namespace

MessageTracker::TrackedMessage::TrackedMessage(
    const MessageDeliveryStage status,
    const MessageDestinationBackgroundType destination_background_type)
    : stage_(status),
      destination_background_type_(destination_background_type) {
  start_time_ = base::Time::Now();
}

void MessageTracker::TrackedMessage::ResetTimeout() {
  start_time_ = base::Time::Now();
}

MessageTracker::MessageDeliveryStage& MessageTracker::TrackedMessage::stage() {
  return stage_;
}

MessageTracker::MessageDestinationBackgroundType
MessageTracker::TrackedMessage::destination_background_type() {
  return destination_background_type_;
}

MessageTracker::MessageTracker(content::BrowserContext* context)
    : context_(context) {}

MessageTracker::~MessageTracker() {
  tracked_messages_.clear();
}

// static
MessageTracker* MessageTracker::Get(content::BrowserContext* browser_context) {
  return static_cast<MessageTrackerFactory*>(GetFactory())
      ->GetForBrowserContext(browser_context);
}

// static
BrowserContextKeyedServiceFactory* MessageTracker::GetFactory() {
  static base::NoDestructor<MessageTrackerFactory> g_factory;
  return g_factory.get();
}

void MessageTracker::NotifyStartTrackingMessageDelivery(
    const base::UnguessableToken& message_id,
    const MessageDeliveryStage stage,
    const MessageDestinationBackgroundType destination_background_type) {
  CHECK(!base::Contains(tracked_messages_, message_id));
  tracked_messages_.emplace(message_id,
                            TrackedMessage(stage, destination_background_type));

  // Eventually emits metrics on whether the message moved to the next stage
  // without going stale.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MessageTracker::NotifyStaleMessage,
                     weak_factory_.GetWeakPtr(), message_id, stage),
      /*delay=*/message_stale_timeout_);
}

void MessageTracker::NotifyUpdateMessageDelivery(
    const base::UnguessableToken& message_id,
    const MessageDeliveryStage new_stage) {
  TrackedMessage* tracked_message = GetTrackedMessage(message_id);

  // A message might've become stale and then later we try updating its stage,
  // but we've removed its tracking due to stale.
  if (!tracked_message) {
    return;
  }

  tracked_message->ResetTimeout();
  // A message should only move "forward" in the messaging stages.
  CHECK(new_stage > tracked_message->stage());
  tracked_message->stage() = new_stage;

  // Eventually emits metrics on whether the message moved to the next stage
  // without hitting the timeout.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MessageTracker::NotifyStaleMessage,
                     weak_factory_.GetWeakPtr(), message_id, new_stage),
      /*delay=*/message_stale_timeout_);
}

void MessageTracker::NotifyStopTrackingMessageDelivery(
    const base::UnguessableToken& message_id) {
  TrackedMessage* tracked_message = GetTrackedMessage(message_id);
  // A message might've become stale and then later we try to stop tracking it,
  // but we've removed its tracking due to stale.
  if (!tracked_message) {
    return;
  }

  base::UmaHistogramBoolean(
      "Extensions.ServiceWorkerBackground.MessageStagesCompleted", true);
  base::UmaHistogramEnumeration(
      "Extensions.ServiceWorkerBackground."
      "MessageStagesCompletedForBackground",
      tracked_message->destination_background_type());
  base::UmaHistogramCustomTimes(
      "Extensions.ServiceWorkerBackground.MessageStagesCompletedTime",
      base::Time::Now() - tracked_message->start_time(), base::Microseconds(1),
      base::Seconds(30), /*buckets=*/50);

  tracked_messages_.erase(message_id);
}

MessageTracker::TestObserver::TestObserver() = default;
MessageTracker::TestObserver::~TestObserver() = default;

// static
void MessageTracker::SetObserverForTest(
    MessageTracker::TestObserver* observer) {
  g_test_observer = observer;
}

MessageTracker::TrackedMessage* MessageTracker::GetTrackedMessage(
    const base::UnguessableToken& message_id) {
  auto it = tracked_messages_.find(message_id);
  return it == tracked_messages_.end() ? nullptr : &it->second;
}

void MessageTracker::NotifyStaleMessage(
    const base::UnguessableToken message_id,
    const MessageDeliveryStage previous_stage) {
  // Ensure the test observer is notified before we exit the method, but
  // after we do any work related to handling the registration.
  base::ScopedClosureRunner notify_test_observer(base::BindOnce(
      [](const base::UnguessableToken& message_id) {
        if (g_test_observer) {
          g_test_observer->OnTrackingStale(message_id);
        }
      },
      message_id));

  TrackedMessage* tracked_message = GetTrackedMessage(message_id);

  // The message is no longer being tracked (e.g. completed process
  // successfully).
  if (!tracked_message) {
    return;
  }

  // Message moved onto the next stage so return early. Another
  // NotifyStaleMessage() will check the new status.
  if (tracked_message->stage() != previous_stage) {
    return;
  }

  // Message is stale so emit fail metrics and cleanup from tracking.
  base::UmaHistogramBoolean(
      "Extensions.ServiceWorkerBackground.MessageStagesCompleted", false);
  base::UmaHistogramEnumeration(
      "Extensions.ServiceWorkerBackground.MessageStaleAtStage",
      tracked_message->stage());
  base::UmaHistogramEnumeration(
      "Extensions.ServiceWorkerBackground.MessageStaleForBackgroundType",
      tracked_message->destination_background_type());
  tracked_messages_.erase(message_id);
}

}  // namespace extensions
