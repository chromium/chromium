// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_CALLBACKS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_CALLBACKS_H_

#include "base/functional/callback.h"

namespace autofill {

// Provides methods for the possible user actions of the VCN enrollment flow.
class VirtualCardEnrollmentCallbacks {
 public:
  VirtualCardEnrollmentCallbacks();
  // Construct this object given all the callbacks.
  VirtualCardEnrollmentCallbacks(base::OnceClosure accept_callback,
                                 base::OnceClosure decline_callback);
  ~VirtualCardEnrollmentCallbacks();

  // VirtualCardEnrollmentCallbacks is only moveable.
  VirtualCardEnrollmentCallbacks(VirtualCardEnrollmentCallbacks&) = delete;
  VirtualCardEnrollmentCallbacks& operator=(VirtualCardEnrollmentCallbacks&) =
      delete;
  VirtualCardEnrollmentCallbacks(VirtualCardEnrollmentCallbacks&& other);
  VirtualCardEnrollmentCallbacks& operator=(
      VirtualCardEnrollmentCallbacks&& other) = default;

  // Completes the accepted virtual card enrollment.
  void OnAccepted();

  // Completes the declined virtual card enrollment.
  void OnDeclined();

 private:
  base::OnceClosure accept_callback_;
  base::OnceClosure decline_callback_;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_CALLBACKS_H_
