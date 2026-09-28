// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_SERVICE_H_
#define IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_SERVICE_H_

#include <Foundation/Foundation.h>

#include "base/functional/callback.h"
#include "base/i18n/language_tag.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/values.h"
#include "ios/chrome/browser/upgrade/model/upgrade_recommended_details.h"

class OmahaBackend;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

// This service handles the communication with the Omaha server.
class OmahaService {
 public:
  // Called when an upgrade is recommended.
  using UpgradeRecommendedCallback =
      base::RepeatingCallback<void(const UpgradeRecommendedDetails&)>;

  // Called when a one-off Omaha check returns.
  using OneOffCallback =
      base::OnceCallback<void(const UpgradeRecommendedDetails&)>;

  // Starts the service using the given SharedURLLoaderFactory. If the callback
  // is set it will be invoked when a ping is received from the server. Calling
  // this method twice is an error.
  static void Start(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      UpgradeRecommendedCallback upgrade_recommended_callback = {});

  // Returns `true` if the Omaha service is available and has been
  // successfully started for this build variant. Returns `false` if
  // the Omaha service is unavailable or not started.
  //
  // Clients should always check if the Omaha service was started by
  // calling this method before invoking `CheckNow()`.
  static bool HasStarted();

  // Request an immediate check with the Omaha server. The callback will
  // be called with the result of the ping.
  static void CheckNow(OneOffCallback callback);

  // Returns debug information about the Omaha service.
  static void GetDebugInformation(
      base::OnceCallback<void(base::DictValue)> callback);

 private:
  // For the singleton:
  friend class base::NoDestructor<OmahaService>;

  // Returns whether Omaha is enabled for this build variant.
  static bool IsEnabled();

  // Raw `GetInstance` method. Necessary for using singletons. This method must
  // only be called if `IsEnabled()` returns true.
  static OmahaService* GetInstance();

  // Default constructor for use by the singleton.
  OmahaService();

  // Creates a service with the given language tag and local state. May be
  // disabled if depending on the build variant.
  OmahaService(const PrefService& local_state,
               const base::i18n::LanguageTag& language_tag);

  OmahaService(const OmahaService&) = delete;
  OmahaService& operator=(const OmahaService&) = delete;

  ~OmahaService();

  // Internal implementation of Start().
  void StartImpl(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      UpgradeRecommendedCallback upgrade_recommended_callback);

  // Internal implementation of HasStarted().
  bool HasStartedImpl() const;

  // Internal implementation of CheckNow().
  void CheckNowImpl(OneOffCallback callback);

  // Internal implementation of GetDebugInformation().
  void GetDebugInformationImpl(
      base::OnceCallback<void(base::DictValue)> callback);

  // Called from the callback passed to OmahaBackend::Start().
  void OnPingReceived(const UpgradeRecommendedDetails& details);

  // OmahaService is sequence-bound.
  SEQUENCE_CHECKER(sequence_checker_);

  // To communicate with the OmahaService.
  base::SequenceBound<OmahaBackend> backend_;

  // The saved UpgradeRecommendedCallback passed to Start().
  UpgradeRecommendedCallback upgrade_recommended_callback_;

  // The saved OneOffCallback passed to CheckNow().
  OneOffCallback one_off_callback_;

  // Whether the service has been started.
  bool started_ = false;

  // Ensure that callbacks won't see dangling pointers.
  base::WeakPtrFactory<OmahaService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_SERVICE_H_
