// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_SERVICE_H_
#define IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "base/version.h"
#include "ios/chrome/browser/upgrade/model/upgrade_recommended_details.h"

class OmahaService;

namespace network {
class SharedURLLoaderFactory;
class PendingSharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

struct UpgradeRecommendedDetails;

// All `OmahaServiceObserver` events will be evaluated on the same sequence the
// `OmahaService` is created on.
class OmahaServiceObserver : public base::CheckedObserver {
 public:
  // Called whenever the Omaha Service determines a change in
  // `UpgradeRecommendedDetails`.
  virtual void UpgradeRecommendedDetailsChanged(
      UpgradeRecommendedDetails details) {}

  // Notifies the observer that `omaha_service` has begun shutting down.
  // Observers should remove themselves from the service via
  // `omaha_service->RemoveObserver(...)` when this happens.
  virtual void ServiceWillShutdown(OmahaService* omaha_service) {}
};

// This service handles the communication with the Omaha server. It also
// handles all the scheduling necessary to contact the server regularly.
// All methods, but the constructor, `GetInstance` and `Start` methods, must be
// called from the IO thread.
class OmahaService {
 public:
  // Called when an upgrade is recommended.
  using UpgradeRecommendedCallback =
      base::RepeatingCallback<void(const UpgradeRecommendedDetails&)>;

  // Called when a one-off Omaha check returns.
  using OneOffCallback = base::OnceCallback<void(UpgradeRecommendedDetails)>;

  // Starts the service. Also set the `URLLoaderFactory` necessary to access the
  // Omaha server. This method should only be called once.  Does nothing if
  // Omaha should not be enabled for this build variant.
  static void Start(std::unique_ptr<network::PendingSharedURLLoaderFactory>
                        pending_url_loader_factory,
                    const UpgradeRecommendedCallback& callback);

  OmahaService(const OmahaService&) = delete;
  OmahaService& operator=(const OmahaService&) = delete;

  // Posts to CheckNowOnIOThread on IO thread to perform an immediate check
  // if the device is up to date.
  static void CheckNow(OneOffCallback callback);

  // Adds/removes an observer to be notified of `OmahaServiceObserver` events.
  static void AddObserver(OmahaServiceObserver* observer);
  static void RemoveObserver(OmahaServiceObserver* observer);
  void RegisterObserver(OmahaServiceObserver* observer);
  void UnregisterObserver(OmahaServiceObserver* observer);

  // Returns debug information about the omaha service.
  static void GetDebugInformation(
      base::OnceCallback<void(base::Value::Dict)> callback);

 private:
  // For tests:
  friend class OmahaServiceTest;
  friend class OmahaServiceInternalTest;
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, PingMessageTest);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest,
                           PingMessageTestWithUnknownInstallDate);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, InstallEventMessageTest);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, SendPingFailure);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, SendPingSuccess);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest,
                           CallbackForScheduledNotUsedOnErrorResponse);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, OneOffSuccess);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, OngoingPingOneOffCallbackUsed);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, OneOffCallbackUsedOnlyOnce);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, ScheduledPingDuringOneOffDropped);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, ParseAndEchoLastServerDate);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, SendInstallEventSuccess);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, SendPingReceiveUpdate);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, PersistStatesTest);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, BackoffTest);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, NonSpammingTest);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, ActivePingAfterInstallEventTest);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, InstallRetryTest);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, PingUpToDateUpdatesUserDefaults);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceTest, PingOutOfDateUpdatesUserDefaults);
  FRIEND_TEST_ALL_PREFIXES(OmahaServiceInternalTest,
                           PingMessageTestWithProfileData);

  // For the singleton:
  friend class base::NoDestructor<OmahaService>;

  // Enum for the `GetPingContent` and `GetNextPingRequestId` method.
  enum PingContent {
    INSTALL_EVENT,
    USAGE_PING,
  };

  // Starts the service. Called on startup. `task_runner` ensures responses from
  // async Omaha requests are posted on the same sequence that `OmahaService`
  // was created on.
  void StartInternal(
      const scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Stops the service in preparation for browser shutdown.
  void StopInternal();

  // URL loader completion callback.
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // Returns whether Omaha is enabled for this build variant.
  static bool IsEnabled();

  // Raw `GetInstance` method. Necessary for using singletons. This method must
  // only be called if `IsEnabled()` returns true.
  static OmahaService* GetInstance();

  // Private constructor, only used by the singleton.
  OmahaService();
  // Private constructor, only used for tests.
  explicit OmahaService(bool schedule);
  ~OmahaService();

  // Returns the time to wait before next attempt.
  static base::TimeDelta GetBackOff(uint8_t number_of_tries);

  void set_upgrade_recommended_callback(
      const UpgradeRecommendedCallback& callback) {
    upgrade_recommended_callback_ = callback;
  }

  // Notifies all observers of the latest `details`.
  void NotifyObservers(UpgradeRecommendedDetails details);

  // Sends a ping to the Omaha server.
  void SendPing();

  // Method that will either start sending a ping to the server, or schedule
  // itself to be called again when the next ping must be send.
  void SendOrScheduleNextPing();

  // Persists the state of the service.
  void PersistStates();

  // Returns the XML representation of the ping message to send to the Omaha
  // server. If `sendInstallEvent` is true, the message will contain an
  // installation complete event.
  std::string GetPingContent(const std::string& requestId,
                             const std::string& sessionId,
                             const std::string& versionName,
                             const std::string& channelName,
                             const base::Time& installationTime,
                             PingContent pingContent);

  // Returns the xml representation of the ping message to send to the Omaha
  // server. Use the current state of the service to compute the right message.
  std::string GetCurrentPingContent();

  // Performs an immediate check to see if the device is up to date. Start must
  // have been previously called.
  void CheckNowOnIOThread(OneOffCallback callback);

  // Computes debugging information and fill `result`.
  void GetDebugInformationOnIOThread(
      base::OnceCallback<void(base::Value::Dict)> callback);

  // Returns whether the next ping to send must a an install/update ping. If
  // `true`, the next ping must use `GetInstallRetryRequestId` as identifier
  // for the request and must include a X-RequestAge header.
  bool IsNextPingInstallRetry();

  // Returns the request identifier to use for the next ping. If it is an
  // install/update retry, it will return the identifier used on the initial
  // request. If this is not the case, returns a random id.
  // `send_install_event` must be true if the next ping is a install/update
  // event, in that case, the identifier will be stored so that it can be
  // reused until the ping is successful.
  std::string GetNextPingRequestId(PingContent ping_content);

  // Stores the given request id to be reused on install/update retry.
  void SetInstallRetryRequestId(const std::string& request_id);

  // Clears the stored request id for a installation/update ping retry. Must be
  // called after a successful installation/update ping.
  void ClearInstallRetryRequestId();

  // Initialize the URLLoaderFactory instance (mostly needed for tests).
  void InitializeURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Clears the all persistent state. Should only be used for testing.
  static void ClearPersistentStateForTests();

  // To communicate with the Omaha server.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Whether the service has been started.
  bool started_;

  // The timer that call this object back when needed.
  base::OneShotTimer timer_;

  // Whether to schedule pings. This is only false for tests.
  const bool schedule_;

  // The install date of the application.  This is fetched in `StartInternal` on
  // the main thread and cached for use on the IO thread.
  int64_t application_install_date_;

  // The time at which the last ping was sent.
  base::Time last_sent_time_;

  // The time at which to send the next ping.
  base::Time next_tries_time_;

  // The timestamp of the ping to send.
  base::Time current_ping_time_;

  // Last version for which an installation ping has been sent.
  base::Version last_sent_version_;

  // Last received server date.
  int last_server_date_;

  // The language in use at start up.
  std::string locale_lang_;

  // Number of tries of the last ping.
  uint8_t number_of_tries_;

  // Whether the ping currently being sent is an install (new or update) ping.
  bool sending_install_event_;

  // If a scheduled ping was canceled.
  bool scheduled_ping_canceled_ = false;

  // Called to notify that upgrade is recommended.
  UpgradeRecommendedCallback upgrade_recommended_callback_;

  // Stores the callback for one off Omaha checks.
  OneOffCallback one_off_check_callback_;

  // Observers to listen to `OmahaService` changes.
  base::ObserverList<OmahaServiceObserver, true> observers_;

  // Validates `OmahaServiceObserver` events are evaluated on the same sequence
  // that `OmahaService` was created on.
  SEQUENCE_CHECKER(sequence_checker_);

  // Ensures responses from async Omaha requests are posted on the same sequence
  // that `OmahaService` was created on.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

#endif  // IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_SERVICE_H_
