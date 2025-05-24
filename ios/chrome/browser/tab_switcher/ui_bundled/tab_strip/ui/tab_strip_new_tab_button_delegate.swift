// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

/// Informs the receiver of actions on the new tab button.
protocol TabStripNewTabButtonDelegate {
  /// Informs the receiver that the new tab button was tapped.
  func newTabButtonTapped()
}
