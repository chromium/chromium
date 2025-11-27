// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import AuthenticationServices
import Foundation

/// Delegate for CredentialImportManager.
@objc public protocol CredentialImportManagerDelegate {
  /// Called when translating the credentials from ASExportedCredentialData to NSObjects is finished.
  @objc func onCredentialsTranslated(
    passwords: [CredentialExchangePassword], passkeys: [CredentialExchangePasskey])
}

/// Handles importing user credentials through ASCredentialImportManager.
@MainActor
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

  /// Delegate for this class.
  @objc weak public var delegate: CredentialImportManagerDelegate?

  /// Begins the credential import process by providing a UUID token received by the OS during the
  /// app launch.
  @available(iOS 26, *)
  @objc public func prepareImport(_ uuid: NSUUID) {
    Task {
      // Initialize an import manager within the scope of Task to prevent an instance from crossing
      // boundaries.
      let importManager = ASCredentialImportManager()

      do {
        let credentialData = try await importManager.importCredentials(token: uuid as UUID)
        let translatedData = translateCredentialData(credentialData)
        delegate?.onCredentialsTranslated(
          passwords: translatedData.passwords, passkeys: translatedData.passkeys)
      } catch {
        // TODO(crbug.com/445889307): Handle errors.
      }
    }
  }

  ///  Translates ASExportedCredentialData into lists of NSObjects used in CredentialImporter.
  @available(iOS 26, *)
  private func translateCredentialData(
    _ credentialData: ASExportedCredentialData
  ) -> (passwords: [CredentialExchangePassword], passkeys: [CredentialExchangePasskey]) {
    var passwords: [CredentialExchangePassword] = []
    var passkeys: [CredentialExchangePasskey] = []

    for account in credentialData.accounts {
      for item in account.items {
        // TODO(crbug.com/445889719): Handle Android app scope as well.
        let optionalUrl = item.scope?.urls.first

        let credentials = item.credentials
        for i in 0..<credentials.count {
          switch credentials[i] {
          case .basicAuthentication(let basicAuth):
            // If the next credential is of type note, treat it as note for password.
            var note = ""
            let nextIndex = i + 1
            if nextIndex < credentials.count {
              if case .note(let noteData) = credentials[nextIndex] {
                note = noteData.content.value
              }
            }
            passwords.append(
              CredentialExchangePassword(
                url: optionalUrl,
                username: basicAuth.userName?.value ?? "",
                password: basicAuth.password?.value ?? "",
                note: note
              ))
          case .passkey(let passkey):
            passkeys.append(
              CredentialExchangePasskey(
                credentialId: passkey.credentialID,
                rpId: passkey.relyingPartyIdentifier,
                userName: passkey.userName,
                userDisplayName: passkey.userDisplayName,
                userId: passkey.userHandle,
                privateKey: passkey.key))
          default:
            // TODO(crbug.com/445889706): Add logging to assess dropped types.
            continue
          }
        }
      }
    }

    return (passwords, passkeys)
  }
}
