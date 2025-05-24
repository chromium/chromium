// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import AuthenticationServices
import CryptoKit
import Foundation

@objcMembers public class PRFInputValues: NSObject {
  public var saltInput1: Data
  public var saltInput2: Data?

  @available(iOS 18.0, *) init(
    inputValues: ASAuthorizationPublicKeyCredentialPRFAssertionInput.InputValues
  ) {
    saltInput1 = inputValues.saltInput1
    saltInput2 = inputValues.saltInput2
  }
}

@objcMembers public class PRFOutputValues: NSObject {
  public var saltOutput1: Data
  public var saltOutput2: Data?

  @available(iOS 18.0, *) @objc public static func fromValues(
    _ outputValues: [Data]
  )
    -> PRFOutputValues?
  {
    return outputValues.isEmpty ? nil : PRFOutputValues(outputValues: outputValues)
  }

  @available(iOS 18.0, *) init(
    outputValues: [Data]
  ) {
    saltOutput1 = outputValues[0]
    saltOutput2 = (outputValues.count > 1) ? outputValues[1] : nil
  }

  @available(iOS 18.0, *) func key1() -> CryptoKit.SymmetricKey {
    return CryptoKit.SymmetricKey(data: saltOutput1)
  }

  @available(iOS 18.0, *) func key2() -> CryptoKit.SymmetricKey? {
    guard let saltInput = saltOutput2 else { return nil }
    return CryptoKit.SymmetricKey(data: saltInput)
  }

  @available(iOS 18.0, *) func toAssertionOutput()
    -> ASAuthorizationPublicKeyCredentialPRFAssertionOutput?
  {
    return ASAuthorizationPublicKeyCredentialPRFAssertionOutput(
      first: key1(), second: key2())
  }

  @available(iOS 18.0, *) func toRegistrationOutput()
    -> ASAuthorizationPublicKeyCredentialPRFRegistrationOutput?
  {
    return ASAuthorizationPublicKeyCredentialPRFRegistrationOutput(
      first: key1(), second: key2())
  }
}

@objcMembers public class PRFData: NSObject {
  public var inputValues: PRFInputValues?
  public var perCredentialInputValues: [Data: PRFInputValues] = [:]
  public var checkForSupport: Bool

  @available(iOS 18.0, *) init(
    input: ASAuthorizationPublicKeyCredentialPRFAssertionInput
  ) {
    checkForSupport = false
    super.init()
    setInputValues(input.inputValues)

    for (credentialID, inputValues) in input.perCredentialInputValues ?? [:] {
      perCredentialInputValues[credentialID] =
        PRFInputValues(inputValues: inputValues)
    }
  }

  @available(iOS 18.0, *) init(
    input: ASAuthorizationPublicKeyCredentialPRFRegistrationInput
  ) {
    checkForSupport = input.shouldCheckForSupport
    super.init()
    setInputValues(input.inputValues)
  }

  @available(iOS 18.0, *) func setInputValues(
    _ values: ASAuthorizationPublicKeyCredentialPRFAssertionInput.InputValues?
  ) {
    guard let prfInputValues = values else { return }
    inputValues = PRFInputValues(inputValues: prfInputValues)
  }

  @available(iOS 18.0, *) @objc public static func fromParameters(
    _ parameters: ASPasskeyCredentialRequestParameters
  )
    -> PRFData?
  {
    guard let prf = parameters.extensionInput?.prf else { return nil }
    return PRFData(input: prf)
  }

  @available(iOS 18.0, *) @objc public static func fromRequest(
    _ request: ASPasskeyCredentialRequest
  )
    -> PRFData?
  {
    switch request.extensionInput {
    case .assertion(let input):
      guard let prf = input.prf else { return nil }
      return PRFData(input: prf)
    case .registration(let input):
      guard let prf = input.prf else { return nil }
      return PRFData(input: prf)
    case .none:
      return nil
    @unknown default:
      return nil
    }
  }
}

@available(iOS 18.0, *) @objc extension ASPasskeyAssertionCredential {
  @objc public func setPRF(fromOutputValues outputValues: PRFOutputValues) {
    guard let prf = outputValues.toAssertionOutput() else { return }
    extensionOutput = ASPasskeyAssertionCredentialExtensionOutput(
      largeBlob: extensionOutput?.largeBlob, prf: prf)
  }
}

@available(iOS 18.0, *) @objc extension ASPasskeyRegistrationCredential {
  @objc public func setPRF(fromOutputValues outputValues: PRFOutputValues) {
    guard let prf = outputValues.toRegistrationOutput() else { return }
    extensionOutput = ASPasskeyRegistrationCredentialExtensionOutput(
      largeBlob: extensionOutput?.largeBlob, prf: prf)
  }

  @objc public func setPRFIsSupported() {
    extensionOutput = ASPasskeyRegistrationCredentialExtensionOutput(
      largeBlob: extensionOutput?.largeBlob,
      prf: ASAuthorizationPublicKeyCredentialPRFRegistrationOutput.supported)
  }
}
