// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MESSAGING_MESSAGE_TRACKER_H_
#define EXTENSIONS_BROWSER_API_MESSAGING_MESSAGE_TRACKER_H_

#include <map>

#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// Tracks an extension message from the browser process as it is sent to a
// background context and emits a metric if the message stays in one stage of
// the message process for too long and becomes "stale".
class MessageTracker : public KeyedService {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum MessageDestinationBackgroundType {
    kNone = 0,
    kPersistentPage = 1,
    kEventPage = 2,
    kServiceWorker = 3,
    kMaxValue = kServiceWorker,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class MessageDeliveryStage {
    kUnknown = 0,
    // Browser received request from renderer to open a channel prior to sending
    // message.
    kOpenChannelRequestReceived = 1,
    // TODO(crbug.com/371011217): Add new values as each stage of the messaging
    // process is tracked.
    kMaxValue = kOpenChannelRequestReceived,
  };

  explicit MessageTracker(content::BrowserContext* context);
  ~MessageTracker() override;

  // Returns the MessageTracker for the given `browser_context`.
  // Note: This class has a global instance across regular and OTR contexts.
  static MessageTracker* Get(content::BrowserContext* browser_context);

  // Returns the KeyedServiceFactory for the MessageTracker.
  static BrowserContextKeyedServiceFactory* GetFactory();

  class TrackedMessage {
   public:
    explicit TrackedMessage(
        const MessageDeliveryStage stage,
        const MessageDestinationBackgroundType destination_background_type);
    ~TrackedMessage() = default;

    void ResetTimeout();
    MessageDeliveryStage& stage();
    MessageDestinationBackgroundType destination_background_type();
    const base::Time& start_time() { return start_time_; }

   private:
    MessageDeliveryStage stage_;
    MessageDestinationBackgroundType destination_background_type_;
    base::Time start_time_;
  };

  // Notifies the tracker that a message is being sent to a background context.
  // The message is tracked until it is delivered or becomes stale.
  // `message_id` is a unique identifier for the message.
  // `stage` is the initial stage of the message delivery process.
  // `destination_background_type` indicates the type of background context
  // the message is being sent to.
  void NotifyStartTrackingMessageDelivery(
      const base::UnguessableToken& message_id,
      const MessageDeliveryStage stage,
      const MessageDestinationBackgroundType destination_background_type_);

  // Notifies the tracker that a message has moved to the next stage in the
  // messaging process. The message is tracked until it is delivered or becomes
  // stale. This emits metrics that the message passed the previous stage
  // successfully.
  void NotifyUpdateMessageDelivery(const base::UnguessableToken& message_id,
                                   const MessageDeliveryStage new_stage);

  // Notifies the tracker that a message has completed the messaging process and
  // should not longer be tracked until it is delivered or becomes stale. This
  // emits metrics that the message passed its current (final) stage
  // successfully.
  void NotifyStopTrackingMessageDelivery(
      const base::UnguessableToken& message_id);

  class TestObserver {
   public:
    TestObserver();

    TestObserver(const TestObserver&) = delete;
    TestObserver& operator=(const TestObserver&) = delete;

    virtual ~TestObserver();

    // Notifies the observer when a message has been detected as stale.
    virtual void OnTrackingStale(const base::UnguessableToken& message_id) {}
  };

  static void SetObserverForTest(TestObserver* observer);

  void SetMessageStaleTimeoutForTest(base::TimeDelta stale_timeout) {
    message_stale_timeout_ = stale_timeout;
  }

  size_t GetNumberOfTrackedMessagesForTest() {
    return tracked_messages_.size();
  }

 private:
  TrackedMessage* GetTrackedMessage(const base::UnguessableToken& message_id);

  // When run the tracked message with `message_id` is considered stale and
  // metrics are emitted
  void NotifyStaleMessage(const base::UnguessableToken message_id,
                          const MessageDeliveryStage stage);

  std::map<base::UnguessableToken, TrackedMessage> tracked_messages_;

  raw_ptr<content::BrowserContext> context_;

  // Since we can't wait forever for a message to not arrive, 30 seconds was
  // chosen arbitrarily as an upper bound for how long until a message is
  // considered to (probably) never progress to the next stage in the messaging
  // process. The class handles if a message does, after this timeout, proceed
  // to the next stage.
  base::TimeDelta message_stale_timeout_ = base::Seconds(30);

  base::WeakPtrFactory<MessageTracker> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MESSAGING_MESSAGE_TRACKER_H_
