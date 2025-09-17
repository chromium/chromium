// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import AuthenticationServices
import Foundation

/// Handles importing user credentials through ASCredentialImportManager.
@objc public class CredentialImportManager: NSObject {
  /// The activity type used in user activity objects sent to importing apps.
  @available(iOS 26, *)
  @objc public static var credentialExchangeActivity: NSString {
    return ASCredentialExchangeActivity as NSString
  }

  /// The key for the token in the user info dictionary of the user activity sent to importing apps.
  @available(iOS 26, *)
  @objc public static var credentialImportToken: NSString {
    return ASCredentialImportToken as NSString
  }

  /// Begins the credential import process by providing a UUID token received by the OS during the
  /// app launch.
  @available(iOS 26, *)
  @objc public func startImport(_ uuid: NSUUID) {
    let importManager = ASCredentialImportManager()
    Task {
      do {
        let _ = try await importManager.importCredentials(token: uuid as UUID)
        // TODO(crbug.com/444149683): Handle received data.
      } catch {
        // TODO(crbug.com/444149683): Handle errors.
      }
    }
  }
}
