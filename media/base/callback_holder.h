// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CALLBACK_HOLDER_H_
#define MEDIA_BASE_CALLBACK_HOLDER_H_

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"

namespace media {

// A helper class that can hold a callback from being fired.
template <typename CB> class CallbackHolder {
 public:
  CallbackHolder() : hold_(false) {}

  ~CallbackHolder() {
    DCHECK(original_cb_.is_null());
    DCHECK(held_cb_.is_null());
  }

  // Sets the callback to be potentially held.
  void SetCallback(CB cb) {
    DCHECK(original_cb_.is_null());
    DCHECK(held_cb_.is_null());
    original_cb_ = std::move(cb);
  }

  bool IsNull() const {
    return original_cb_.is_null() && held_cb_.is_null();
  }

  // Holds the callback when Run() is called.
  void HoldCallback() { hold_ = true; }

  // Runs or holds the callback as specified by |hold_|.
  void RunOrHold() {
    DCHECK(held_cb_.is_null());
    if (hold_)
      held_cb_ = std::move(original_cb_);
    else
      std::move(original_cb_).Run();
  }

  template <typename... Args>
  void RunOrHold(Args&&... args) {
    DCHECK(held_cb_.is_null());
    if (hold_) {
      held_cb_ =
          base::BindOnce(std::move(original_cb_), std::forward<Args>(args)...);
    } else {
      std::move(original_cb_).Run(std::forward<Args>(args)...);
    }
  }

  // Releases and runs the held callback.
  void RunHeldCallback() {
    DCHECK(hold_);
    DCHECK(!held_cb_.is_null());
    hold_ = false;
    std::move(held_cb_).Run();
  }

 private:
  bool hold_;
  CB original_cb_;
  base::OnceClosure held_cb_;
};

}  // namespace media

#endif  // MEDIA_BASE_CALLBACK_HOLDER_H_
