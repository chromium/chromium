// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_BACKEND_H_
#define IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_BACKEND_H_

#import <string>

#import "base/functional/callback.h"
#import "base/memory/ref_counted.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "base/values.h"
#import "ios/chrome/browser/omaha/model/omaha_persistent_state.h"
#import "url/gurl.h"

enum class OmahaPingEvent;
struct UpgradeRecommendedDetails;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

// Handles the communication with the Omaha service taking care of the
// scheduling necessary to contact the server regularly. Must only be
// used on a sequence where blocking is allowed.
class OmahaBackend {
 public:
  // Callback to create a SharedURLLoaderFactory.
  using PendingURLLoaderFactoryCallback =
      base::OnceCallback<scoped_refptr<network::SharedURLLoaderFactory>()>;

  // Callback invoked with upgrade recommendation details.
  using UpgradeRecommendedCallback =
      base::RepeatingCallback<void(const UpgradeRecommendedDetails&)>;

  // Callback used to save the persistent state of the OmahaBackend.
  using SavePersistentStateCallback =
      base::RepeatingCallback<void(const OmahaPersistentState&)>;

  // Create the instance with constant state.
  OmahaBackend(std::string locale_lang,
               base::Time app_install,
               GURL omaha_server_url,
               bool auto_schedule);

  OmahaBackend(const OmahaBackend&) = delete;
  OmahaBackend& operator=(const OmahaBackend&) = delete;

  ~OmahaBackend();

  // Starts the communication with the Omaha server.
  void Start(OmahaPersistentState initial_state,
             PendingURLLoaderFactoryCallback pending_url_loader_factory,
             UpgradeRecommendedCallback upgrade_recommended_callback,
             SavePersistentStateCallback save_persistent_state_callback);

  // Performs an immediate check to see if the device is up to date.
  void CheckNow();

  // Returns debugging information.
  base::DictValue GetDebugInformation() const;

 private:
  // Schedules sending the next ping.
  void ScheduleNextPing();

  // Sends a ping to the Omaha server.
  void SendPing();

  // Returns the content for a ping.
  std::string GetPingContent(OmahaPingEvent event,
                             base::Version version,
                             std::string request_id) const;

  // Returns the content for a default ping (for debug).
  std::string GetDefaultPingContent() const;

  // Resynchronizes the timer if device sleep has caused it to drift.
  void ResyncTimerIfNeeded();

  // Persists the current state to storage.
  void PersistState() const;

  // Returns the ping event for `version`.
  OmahaPingEvent GetPingEventForVersion(const base::Version& version) const;

  // Invoked when the response from the Omaha server is received.
  void OnServerResponse(std::optional<std::string> response_body);

  SEQUENCE_CHECKER(sequence_checker_);

  // The language in use at startup.
  const std::string locale_lang_;

  // The application install date.
  const base::Time app_install_;

  // The URL of the omaha server. Must be valid.
  const GURL omaha_server_url_;

  // Used to communicate with the Omaha server.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> current_url_loader_;

  // Callback used to inform of upgrade recommended details.
  UpgradeRecommendedCallback upgrade_recommended_callback_;

  // Callback used to save the current state of the service.
  SavePersistentStateCallback save_persistent_state_callback_;

  // The current persisted state.
  OmahaPersistentState current_state_;

  // Timer used to schedule pings.
  base::OneShotTimer timer_;

  // Opaque handle used to cancel the registration with NSNotificationCenter.
  id notification_registration_handle_;

  // Whether the server can schedule ping automatically (may be false during
  // tests as simulating network response does not play well with timers).
  const bool auto_schedule_;

  // To ensure the notification callback is cancelled if the object is
  // destroyed before it executes.
  base::WeakPtrFactory<OmahaBackend> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_BACKEND_H_
