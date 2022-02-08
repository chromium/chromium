// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

@objcMembers class Pedal: NSObject {
  let title: String

  init(title: String) {
    self.title = title
  }
}
