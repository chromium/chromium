// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_FAKE_SAFE_BROWSING_CLIENT_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_FAKE_SAFE_BROWSING_CLIENT_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "base/run_loop.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"

class FakeSafeBrowsingService;

// Fake implementation of SafeBrowsingClient.
class FakeSafeBrowsingClient : public SafeBrowsingClient {
 public:
  FakeSafeBrowsingClient();
  ~FakeSafeBrowsingClient() override;

  // SafeBrowsingClient implementation.
  base::WeakPtr<SafeBrowsingClient> AsWeakPtr() override;

  // Controls the return value of `ShouldBlockUnsafeResource`.
  void set_should_block_unsafe_resource(bool should_block_unsafe_resource) {
    should_block_unsafe_resource_ = should_block_unsafe_resource;
  }

  // Controls the return value of `GetRealTimeUrlLookupService`.
  void set_real_time_url_lookup_service(
      safe_browsing::RealTimeUrlLookupService* lookup_service) {
    lookup_service_ = lookup_service;
  }

  // Whether `OnMainFrameUrlQueryCancellationDecided` was called.
  bool main_frame_cancellation_decided_called() {
    return main_frame_cancellation_decided_called_;
  }

  // Pauses sync responses to resume at a later point.
  void pause_sync_response() { pause_sync_check_ = true; }

  // Checks if logic should `pause_sync_check_`.
  bool is_sync_response_paused() { return pause_sync_check_; }

  // Stores a sync callback in `sync_completion_callbacks_` to be ran at a later
  // point.
  void store_sync_callback(
      base::OnceCallback<void()> sync_completion_callback) {
    sync_completion_callbacks_.push_back(std::move(sync_completion_callback));
  }

  // Runs all sync callbacks stored in `sync_completion_callbacks_`.
  void run_sync_callbacks() {
    for (auto& callback : sync_completion_callbacks_) {
      std::move(callback).Run();
    }
  }

  // Pauses async responses to resume at a later point.
  void pause_async_response() { pause_async_check_ = true; }

  // Checks if logic should `pause_async_check_`.
  bool is_async_response_paused() { return pause_async_check_; }

  // Stores a async callback in `async_completion_callbacks_` to be ran at a
  // later point.
  void store_async_callback(
      base::OnceCallback<void()> async_completion_callback) {
    async_completion_callbacks_.push_back(std::move(async_completion_callback));
  }

  // Runs all async callbacks stored in `async_completion_callbacks_`.
  void run_async_callbacks() {
    for (auto& callback : async_completion_callbacks_) {
      std::move(callback).Run();
    }
  }

  bool pause_sync_check_ = false;
  bool pause_async_check_ = false;
  std::vector<base::OnceCallback<void()>> sync_completion_callbacks_;
  std::vector<base::OnceCallback<void()>> async_completion_callbacks_;

 private:
  // SafeBrowsingClient implementation.
  SafeBrowsingService* GetSafeBrowsingService() override;
  safe_browsing::RealTimeUrlLookupService* GetRealTimeUrlLookupService()
      override;
  safe_browsing::HashRealTimeService* GetHashRealTimeService() override;
  variations::VariationsService* GetVariationsService() override;
  bool ShouldBlockUnsafeResource(
      const security_interstitials::UnsafeResource& resource) const override;
  void OnMainFrameUrlQueryCancellationDecided(web::WebState* web_state,
                                              const GURL& url) override;

  scoped_refptr<FakeSafeBrowsingService> safe_browsing_service_;
  bool should_block_unsafe_resource_ = false;
  raw_ptr<safe_browsing::RealTimeUrlLookupService> lookup_service_ = nullptr;
  bool main_frame_cancellation_decided_called_ = false;

  // Must be last.
  base::WeakPtrFactory<FakeSafeBrowsingClient> weak_factory_{this};
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_FAKE_SAFE_BROWSING_CLIENT_H_
