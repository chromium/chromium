// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/message_tracker.h"

#include <map>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"

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
      context);
}

std::unique_ptr<KeyedService>
MessageTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<MessageTracker>(context);
}

const char* GetChannelTypeMetricSuffix(const mojom::ChannelType channel_type) {
  switch (channel_type) {
    case mojom::ChannelType::kSendMessage:
      return "SendMessageChannel";
    case mojom::ChannelType::kConnect:
      return "ConnectChannel";
    case mojom::ChannelType::kNative:
      return "NativeChannel";
    case mojom::ChannelType::kSendRequest:
      return "SendRequestChannel";
  }
}

}  // namespace

MessageTracker::TrackedStage::TrackedStage(std::string metric_name,
                                           mojom::ChannelType channel_type)
    : metric_name_(std::move(metric_name)), channel_type_(channel_type) {}

MessageTracker::MessageTracker(content::BrowserContext* context)
    : context_(context) {}

MessageTracker::~MessageTracker() = default;

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

void MessageTracker::StartTrackingMessagingStage(
    const base::UnguessableToken& tracking_id,
    std::string base_metric_name,
    mojom::ChannelType channel_type) {
  CHECK(!base::Contains(tracked_stages_, tracking_id));
  tracked_stages_.emplace(
      tracking_id, TrackedStage(std::move(base_metric_name), channel_type));

  // Eventually emits metrics on whether the message sat in this stage past the
  // timeout.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MessageTracker::OnMessageTimeoutElapsed,
                     weak_factory_.GetWeakPtr(), tracking_id),
      /*delay=*/stage_timeout_);
}

void MessageTracker::StopTrackingMessagingStage(
    const base::UnguessableToken& message_id,
    OpenChannelMessagePipelineResult result) {
  TrackedStage* tracked_stage = GetTrackedStage(message_id);
  // A message might've been delayed too long and already cleared or two paths
  // might try to stop tracking for `message_id` and one of them finished first.
  if (!tracked_stage) {
    return;
  }

  // Emit overall metric for all channel types and then for the specific type.
  base::UmaHistogramEnumeration(tracked_stage->metric_name(), result);
  const std::string metric_name = base::StringPrintf(
      "%s.%s", tracked_stage->metric_name(),
      GetChannelTypeMetricSuffix(tracked_stage->channel_type()));
  base::UmaHistogramEnumeration(metric_name, result);

  tracked_stages_.erase(message_id);
}

MessageTracker::TestObserver::TestObserver() = default;
MessageTracker::TestObserver::~TestObserver() = default;

// static
void MessageTracker::SetObserverForTest(
    MessageTracker::TestObserver* observer) {
  g_test_observer = observer;
}

MessageTracker::TrackedStage* MessageTracker::GetTrackedStage(
    const base::UnguessableToken& message_id) {
  auto it = tracked_stages_.find(message_id);
  return it == tracked_stages_.end() ? nullptr : &it->second;
}

void MessageTracker::OnMessageTimeoutElapsed(
    const base::UnguessableToken& message_id) {
  // Ensure the test observer is notified before we exit the method, but
  // after we do any work related to handling the registration.
  base::ScopedClosureRunner notify_test_observer(base::BindOnce(
      [](const base::UnguessableToken& message_id) {
        if (g_test_observer) {
          g_test_observer->OnStageTimeoutRan(message_id);
        }
      },
      message_id));

  TrackedStage* tracked_stage = GetTrackedStage(message_id);

  // The message is no longer being tracked (e.g. completed process
  // successfully).
  if (!tracked_stage) {
    return;
  }

  // Message is delayed too long so emit fail metrics and cleanup from tracking.

  // Emit overall metric for all channel types and then for the specific type.
  base::UmaHistogramEnumeration(tracked_stage->metric_name(),
                                OpenChannelMessagePipelineResult::kHung);
  const std::string metric_name = base::StringPrintf(
      "%s.%s", tracked_stage->metric_name(),
      GetChannelTypeMetricSuffix(tracked_stage->channel_type()));

  base::UmaHistogramEnumeration(metric_name,
                                OpenChannelMessagePipelineResult::kHung);
  tracked_stages_.erase(message_id);
}

}  // namespace extensions
