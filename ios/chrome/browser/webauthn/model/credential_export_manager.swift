// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import AuthenticationServices
import Foundation
import UIKit

/// Handles exporting user credentials through ASCredentialExportManager.
@objc public class CredentialExportManager: NSObject {
  /// Begins the credential exchange process by requesting the export options, which triggers the
  /// system UI allowing the user to pick the import credential manager.
  @objc public func startExport(_ window: UIWindow) {
    if #available(iOS 26, *) {
      let exportManager = ASCredentialExportManager(presentationAnchor: window)
      Task {
        do {
          let _ = try await exportManager.requestExport()
          // TODO(crbug.com/444149683): Fetch and export credential data.
        } catch {
          // TODO(crbug.com/444149683): Handle errors.
        }
      }
    }
  }
}
