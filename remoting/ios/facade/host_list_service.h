// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_FACADE_HOST_LIST_SERVICE_H_
#define REMOTING_IOS_FACADE_HOST_LIST_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "net/http/http_status_code.h"
#include "remoting/proto/remoting/v1/directory_messages.pb.h"

namespace grpc {
class Status;
}  // namespace grpc

namespace remoting {

class DirectoryClient;
class OAuthTokenGetter;

// |HostListService| is the centralized place to retrieve the current signed in
// user's host list.
class HostListService {
 public:
  enum class State {
    // Nobody has requested a host list fetch since login or last failure.
    NOT_FETCHED,
    // The host list is currently being fetched.
    FETCHING,
    // The host list has been fetched.
    FETCHED,
  };

  enum class FetchFailureReason {
    NETWORK_ERROR,
    AUTH_ERROR,
    UNKNOWN_ERROR,
  };

  struct FetchFailureInfo {
    FetchFailureReason reason;
    std::string localized_description;
  };

  using CallbackSubscription = base::CallbackList<void()>::Subscription;

  // Returns the singleton instance.
  static HostListService* GetInstance();

  ~HostListService();

  // Registers callback to be called when the host list state is changed.
  std::unique_ptr<CallbackSubscription> RegisterHostListStateCallback(
      const base::RepeatingClosure& callback);

  // Registers callback to be called when the host list has failed to fetch.
  std::unique_ptr<CallbackSubscription> RegisterFetchFailureCallback(
      const base::RepeatingClosure& callback);

  // Start a request to fetch the host list. Calls either the host list state
  // callbacks or fetch failure callbacks during the process.
  void RequestFetch();

  // Returns the host list. Returns an empty vector if the host list state is
  // not |FETCHED|.
  const std::vector<apis::v1::HostInfo>& hosts() const { return hosts_; }

  State state() const { return state_; }

  // Returns the last host list fetch failure. Returns nullptr if the host list
  // has never been fetched or the last fetch has succeeded.
  const FetchFailureInfo* last_fetch_failure() const {
    return last_fetch_failure_.get();
  }

 private:
  friend class base::NoDestructor<HostListService>;
  friend class HostListServiceTest;

  HostListService();

  // For test.
  explicit HostListService(std::unique_ptr<DirectoryClient> directory_client);

  void Init();

  // Changes the host list state and notifies callbacks.
  void SetState(State state);

  void HandleHostListResult(const grpc::Status& status,
                            const apis::v1::GetHostListResponse& response);
  void HandleFetchFailure(const grpc::Status& status);

  void OnUserUpdated(bool is_user_signed_in);

  id user_update_observer_;

  base::CallbackList<void()> host_list_state_callbacks_;
  base::CallbackList<void()> fetch_failure_callbacks_;

  std::unique_ptr<OAuthTokenGetter> token_getter_;
  std::unique_ptr<DirectoryClient> directory_client_;

  std::vector<apis::v1::HostInfo> hosts_;
  State state_ = State::NOT_FETCHED;
  std::unique_ptr<FetchFailureInfo> last_fetch_failure_;

  base::WeakPtrFactory<HostListService> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(HostListService);
};

}  // namespace remoting

#endif  // REMOTING_IOS_FACADE_HOST_LIST_SERVICE_H_
