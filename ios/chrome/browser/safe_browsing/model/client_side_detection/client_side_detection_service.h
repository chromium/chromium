// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_SERVICE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/safe_browsing/core/browser/client_side_detection_service_base.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace safe_browsing {

class Scorer;

// iOS implementation of `ClientSideDetectionService`.
// This class is responsible for holding the phishing model and sending
// phishing reports to Safe Browsing servers.
class ClientSideDetectionService : public ClientSideDetectionServiceBase {
 public:
  ClientSideDetectionService(
      PrefService* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      optimization_guide::OptimizationGuideModelProvider* opt_guide);

  ClientSideDetectionService(const ClientSideDetectionService&) = delete;
  ClientSideDetectionService& operator=(const ClientSideDetectionService&) =
      delete;

  ~ClientSideDetectionService() override;

  // `Observer` interface to listen to `Scorer` updates.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnScorerChanged() = 0;
  };

  // Adds an observer to listen for changes to the `Scorer` (e.g. when a new
  // model is loaded or the service is disabled).
  void AddObserver(Observer* observer);

  // Removes a previously added observer that was listening for changes to the
  // `Scorer`.
  void RemoveObserver(Observer* observer);

  // Returns the `Scorer` associated with this service. This is specific to iOS
  // because classification occurs in the browser process using this `Scorer`.
  // In contrast, the Content implementation delegates classification to
  // renderer processes and does not need to expose a `Scorer` interface here.
  Scorer* GetScorer() const;

  // Sets a mock `Scorer` for testing (or nullptr to clear it) and notifies
  // observers.
  void SetScorerForTesting(std::unique_ptr<Scorer> scorer);

 private:
  friend class ClientSideDetectionServiceTest;

  // `ClientSideDetectionServiceBase` overrides:
  void DidSendClientReportPhishingRequest(
      std::unique_ptr<ClientPhishingRequest> request,
      const std::string& access_token) override;
  void DidReceiveClientPhishingResponse(
      const ClientPhishingResponse& response) override;
  void OnModelUpdated() override;

  // Callback invoked on the UI thread after the `Scorer` has been successfully
  // created on a background thread.
  void OnScorerCreated(int generation_id, std::unique_ptr<Scorer> scorer);

  // Sets the active `Scorer` and notifies observers. Posts destruction of the
  // previous `Scorer` to a background thread because closing file handles
  // performs file I/O operations, which are forbidden on the main thread.
  void SetScorer(std::unique_ptr<Scorer> scorer);

  // Called when Safe Browsing is disabled or the model becomes unavailable.
  void ClearScorerAndNotifyObservers();

  // Tracks the current model generation to ensure that asynchronous background
  // `Scorer` creation tasks do not overwrite the `scorer_` with a stale version
  // if a new model arrives while the task is running. Note: because model
  // updates occur at low frequency, `current_model_generation_` is safe from
  // overflow.
  int current_model_generation_ = 0;
  std::unique_ptr<Scorer> scorer_;
  base::ObserverList<Observer> observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ClientSideDetectionService> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_SERVICE_H_
