// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MESSAGE_TRACKER_H_
#define EXTENSIONS_BROWSER_MESSAGE_TRACKER_H_

#include <map>

#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// Tracks an extension message from the browser process as it's sent to a
// background context and emits metrics on whether it succeeds or fails.
class MessageTracker : public KeyedService {
 public:
  enum class OpenChannelMessagePipelineResult {
    kUnknown = 0,

    // At least one endpoint accepted the connection and the channel was opened.
    kOpened = 1,

    // Multi-use value meaning that the stage did not occur before
    // stage_timeout_ was reached.
    kHung = 2,

    // The channel was not opened due to one of theses issues. See enums.xml for
    // more details (part 1).
    kNoReceivers = 3,
    kOpenChannelToNonEnabledExtension = 4,
    kNotExternallyConnectable = 5,
    kWorkerStarted = 6,
    kWillNotOpenChannel = 7,
    kOpenChannelReceiverInvalidPort = 8,
    kOpenChannelDispatchNoReceivers = 9,

    // The DispatchConnect IPC was acknowledged back to the browser.
    kOpenChannelAcked = 10,
    // The DispatchConnect IPC was not acknowledged because the remote
    // (renderer) was disconnected.
    kOpenChannelPortDisconnectedBeforeResponse = 11,
    // The DispatchConnect IPC was not acknowledged because the channel closed.
    kOpenChannelClosedBeforeResponse = 12,

    // The channel was not opened due to one of theses issues. See enums.xml for
    // more details (part 2).
    kOpenChannelSourceEndpointInvalid = 13,
    kOpenChannelOpenerPortInvalid = 14,
    kOnOpenChannelSourceInvalid = 15,
    kOnOpenChannelOpenerPortInvalid = 16,
    kOnOpenChannelExtensionNotEnabled = 17,

    kMaxValue = kOnOpenChannelExtensionNotEnabled,
  };

  class TestObserver {
   public:
    TestObserver();

    TestObserver(const TestObserver&) = delete;
    TestObserver& operator=(const TestObserver&) = delete;

    virtual ~TestObserver();

    // Notifies the observer when the hung detection for a message ran (but
    // doesn't guarantee the the message was hung).
    virtual void OnStageTimeoutRan(const base::UnguessableToken& message_id) {}
  };

  explicit MessageTracker(content::BrowserContext* context);
  ~MessageTracker() override;

  // Returns the MessageTracker for the given `browser_context`.
  // Note: This class has a global instance across regular and OTR contexts.
  static MessageTracker* Get(content::BrowserContext* browser_context);

  // Returns the KeyedServiceFactory for the MessageTracker.
  static BrowserContextKeyedServiceFactory* GetFactory();

  class TrackedStage {
   public:
    TrackedStage(std::string metric_name, mojom::ChannelType channel_type);
    ~TrackedStage() = default;

    const std::string& metric_name() const { return metric_name_; }
    const mojom::ChannelType& channel_type() const { return channel_type_; }

   private:
    const std::string metric_name_;
    const mojom::ChannelType channel_type_;
  };

  // Notifies the tracker that a message is being sent to a background context.
  // This starts a timer for `tracking_id` that will emit failure metrics if
  // `StopTrackingMessagingStage()` is not called before within
  // `stage_timeout_`.
  void StartTrackingMessagingStage(const base::UnguessableToken& tracking_id,
                                   std::string metric_name,
                                   mojom::ChannelType channel_type);

  // Notifies the tracker that a message has successfully completed the
  // messaging stage and should not longer be tracked. This emits success
  // metrics.
  void StopTrackingMessagingStage(const base::UnguessableToken& message_id,
                                  OpenChannelMessagePipelineResult result);

  static void SetObserverForTest(TestObserver* observer);

  void SetStageHungTimeoutForTest(base::TimeDelta hung_timeout) {
    stage_timeout_ = hung_timeout;
  }

  size_t GetNumberOfTrackedStagesForTest() const {
    return tracked_stages_.size();
  }

 private:
  TrackedStage* GetTrackedStage(const base::UnguessableToken& message_id);

  // When run, the tracked message with `message_id` is considered hung and
  // metrics are emitted.
  void OnMessageTimeoutElapsed(const base::UnguessableToken& message_id);

  // The main container for the messaging metrics data.
  std::map<base::UnguessableToken, TrackedStage> tracked_stages_;

  raw_ptr<content::BrowserContext> context_;

  // Since we can't wait forever for a message to not arrive, 30 seconds was
  // chosen as an upper bound for how long until a stage is considered to
  // (probably) never progress to the next stage in the messaging process.
  base::TimeDelta stage_timeout_ = base::Seconds(30);

  base::WeakPtrFactory<MessageTracker> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MESSAGE_TRACKER_H_
