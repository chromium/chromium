// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_MODEL_FAVICON_WEB_STATE_DISPATCHER_IMPL_H_
#define IOS_CHROME_BROWSER_READING_LIST_MODEL_FAVICON_WEB_STATE_DISPATCHER_IMPL_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/reading_list/ios/favicon_web_state_dispatcher.h"

class ProfileIOS;
namespace web {
class WebState;
class WebStateID;
}

namespace reading_list {

// Implementation of the FaviconWebStateDispatcher.
class FaviconWebStateDispatcherImpl : public FaviconWebStateDispatcher {
 public:
  // Constructor for keeping the WebStates alive for `keep_alive`.
  FaviconWebStateDispatcherImpl(ProfileIOS* profile,
                                base::TimeDelta keep_alive);

  // Constructor using a default value for `keep_alive`.
  explicit FaviconWebStateDispatcherImpl(ProfileIOS* profile);
  ~FaviconWebStateDispatcherImpl() override;

  // FaviconWebStateDispatcher implementation.
  std::unique_ptr<web::WebState> RequestWebState() override;
  void ReturnWebState(std::unique_ptr<web::WebState> web_state) override;
  void ReleaseAll() override;

 private:
  // Release the WebState with given identifier.
  void ReleaseWebStateWithId(web::WebStateID web_state_id);

  // The profile.
  raw_ptr<ProfileIOS> profile_;
  // Map of the WebStates currently alive.
  std::map<web::WebStateID, std::unique_ptr<web::WebState>> web_states_;
  // Time during which the WebState will be kept alive after being returned.
  base::TimeDelta keep_alive_;
  // Factory for weak pointers.
  base::WeakPtrFactory<FaviconWebStateDispatcherImpl> weak_ptr_factory_{this};
};

}  // namespace reading_list

#endif  // IOS_CHROME_BROWSER_READING_LIST_MODEL_FAVICON_WEB_STATE_DISPATCHER_IMPL_H_
