// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

@objcMembers public class PopupModel: NSObject, ObservableObject {
  @Published var matches: [PopupMatch]

  public init(matches: [PopupMatch]) {
    self.matches = matches
  }
}
