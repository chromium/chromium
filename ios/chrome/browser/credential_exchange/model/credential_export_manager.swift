// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import AuthenticationServices
import Foundation
import UIKit

/// Delegate for CredentialExportManager.
@objc public protocol CredentialExportManagerDelegate {
  /// Called when the export failed with an error.
  @objc func onExportError()
}

/// Handles exporting user credentials through ASCredentialExportManager.
@MainActor
@objc public class CredentialExportManager: NSObject {
  private struct ExportablePassword {
    let url: URL
    let username: String
    let password: String
    let host: String
    let note: String?

    init?(_ cred: CredentialExchangePassword) {
      guard let url = cred.url,
        let username = cred.username,
        let password = cred.password,
        let host = url.host
      else {
        return nil
      }
      self.url = url
      self.username = username
      self.password = password
      self.host = host
      self.note = cred.note
    }
  }

  private struct ExportablePasskey {
    let credentialId: Data
    let rpId: String
    let userName: String
    let userDisplayName: String?
    let userId: Data
    let privateKey: Data

    init?(_ key: CredentialExchangePasskey) {
      self.credentialId = key.credentialId
      self.rpId = key.rpId
      self.userName = key.userName
      self.userDisplayName = key.userDisplayName
      self.userId = key.userId
      self.privateKey = key.privateKey
    }
  }

  /// Delegate for this class.
  @objc weak public var delegate: CredentialExportManagerDelegate?

  /// Converts credential data into the `ASExportedCredentialData` format.
  @available(iOS 26, *)
  private static func buildExportData(
    from passwords: [ExportablePassword], and passkeys: [ExportablePasskey],
    and userEmail: String, and exporterName: String
  ) async
    -> ASExportedCredentialData
  {
    var importableItems: [ASImportableItem] = []
    for password in passwords {
      let userField = ASImportableEditableField(
        id: nil, fieldType: .string, value: password.username, label: nil)

      let passField = ASImportableEditableField(
        id: nil, fieldType: .concealedString, value: password.password, label: nil)
      let basicAuth = ASImportableCredential.BasicAuthentication(
        userName: userField,
        password: passField
      )
      var credentialsToExport: [ASImportableCredential] = [.basicAuthentication(basicAuth)]
      if let note = password.note, !note.isEmpty {
        let noteField = ASImportableEditableField(
          id: nil, fieldType: .string, value: note, label: nil)

        let noteData = ASImportableCredential.Note(content: noteField)
        credentialsToExport.append(.note(noteData))
      }
      let scope = ASImportableCredentialScope(urls: [password.url])
      let item = ASImportableItem(
        // TODO(crbug.com/447142330): Replace placeholder data: created, lastModified.
        id: UUID().uuidString.data(using: .utf8)!,
        created: Date(),
        lastModified: Date(),
        title: password.host,
        subtitle: nil,
        favorite: false,
        scope: scope,
        credentials: credentialsToExport,
        tags: []
      )
      importableItems.append(item)
    }

    for passkey in passkeys {
      let passkeyCredential = ASImportableCredential.Passkey(
        credentialID: passkey.credentialId,
        relyingPartyIdentifier: passkey.rpId,
        userName: passkey.userName,
        userDisplayName: passkey.userDisplayName ?? "",
        userHandle: passkey.userId,
        key: passkey.privateKey
      )

      let item = ASImportableItem(
        id: UUID().uuidString.data(using: .utf8)!,
        created: Date(),
        lastModified: Date(),
        title: passkey.rpId,
        subtitle: nil,
        favorite: false,
        scope: nil,
        credentials: [.passkey(passkeyCredential)],
        tags: []
      )
      importableItems.append(item)
    }

    let account = ASImportableAccount(
      id: UUID().uuidString.data(using: .utf8)!,
      userName: "",  // No user-defined pseudonym for the account in GPM.
      email: userEmail,
      fullName: nil,
      collections: [],
      items: importableItems
    )
    let exportedData = ASExportedCredentialData(
      accounts: [account],
      formatVersion: .v1,
      exporterRelyingPartyIdentifier: "passwords.google.com",
      exporterDisplayName: exporterName,
      timestamp: Date()
    )
    return exportedData
  }

  /// Begins the credential exchange process by requesting the export options, which triggers the
  /// system UI allowing the user to pick the import credential manager.
  @available(iOS 26, *)
  @objc(startExportWithPasswords:passkeys:window:userEmail:exporterName:)
  public func startExport(
    passwords: [CredentialExchangePassword], passkeys: [CredentialExchangePasskey],
    window: UIWindow, userEmail: String, exporterName: String
  ) {
    Task { @MainActor in
      do {
        // Initialize an export manager within the scope of Task to prevent an instance from
        // crossing boundaries.
        let exportManager = ASCredentialExportManager(presentationAnchor: window)
        let exportablePasswords = passwords.compactMap(ExportablePassword.init)
        let exportablePasskeys = passkeys.compactMap(ExportablePasskey.init)

        let _ = try await exportManager.requestExport(for: nil)

        let exportedData = await CredentialExportManager.buildExportData(
          from: exportablePasswords, and: exportablePasskeys, and: userEmail, and: exporterName)

        try await exportManager.exportCredentials(exportedData)
      } catch {
        delegate?.onExportError()
      }
    }
  }
}
