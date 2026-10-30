// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/crb_protocol_observers_util.h"

#import "base/ios/crb_protocol_observers.h"

namespace actor {

bool ContainsObserver(CRBProtocolObservers* observers, id observer) {
  __block bool contains = false;
  [observers executeOnObservers:^(id candidate) {
    contains |= candidate == observer;
  }];
  return contains;
}

}  // namespace actor
