'use strict';

// Converts an ECMAScript String object to an instance of
// mojo_base.mojom.String16.
function stringToMojoString16(string) {
  let array = new Array(string.length);
  for (var i = 0; i < string.length; ++i) {
    array[i] = string.charCodeAt(i);
  }
  return { data: array }
}

// Mocks the CredentialManager interface defined in credential_manager.mojom.
class MockCredentialManager {
  constructor() {
    this.reset();

    this.interceptor_ = new MojoInterfaceInterceptor(
        blink.mojom.CredentialManager.$interfaceName);
    this.interceptor_.oninterfacerequest = e => {
      this.bindHandleToReceiver(e.handle);
    };
    this.interceptor_.start();
  }

  bindHandleToReceiver(handle) {
    this.receiver_ = new blink.mojom.CredentialManagerReceiver(this);
    this.receiver_.$.bindHandle(handle);
  }

  constructCredentialInfo_(type, id, password, name, icon) {
    return {
      type: type,
      id: stringToMojoString16(id),
      name: stringToMojoString16(name),
      icon: {url: icon},
      password: stringToMojoString16(password),
      federation: {scheme: 'https', host: 'foo.com', port: 443}
    };
  }

  // Mock functions:
  async get(mediation, includePasswords, federations) {
    if (this.error_ == blink.mojom.CredentialManagerError.SUCCESS) {
      return {error: this.error_, credential: this.credentialInfo_};
    } else {
      return {error: this.error_, credential: null};
    }
  }

  async store(credential) {
    return {};
  }

  async preventSilentAccess() {
    return {};
  }

  // Resets state of mock CredentialManager.
  reset() {
    this.error_ = blink.mojom.CredentialManagerError.SUCCESS;
    this.credentialInfo_ = this.constructCredentialInfo_(
        blink.mojom.CredentialType.EMPTY, '', '', '', '');
  }

  setResponse(id, password, name, icon) {
    this.credentialInfo_ = this.constructCredentialInfo_(
        blink.mojom.CredentialType.PASSWORD, id, password, name,
        icon);
  }

  setError(error) {
    this.error_ = error;
  }
}

// Class that mocks Authenticator interface defined in authenticator.mojom.
class MockAuthenticator {
  constructor() {
    this.reset();

    this.interceptor_ =
        new MojoInterfaceInterceptor(blink.mojom.Authenticator.$interfaceName);
    this.interceptor_.oninterfacerequest = e => {
      this.bindHandleToReceiver(e.handle);
    };
    this.interceptor_.start();
  }

  bindHandleToReceiver(handle) {
    this.receiver_ = new blink.mojom.AuthenticatorReceiver(this);
    this.receiver_.$.bindHandle(handle);
  }

  // Returns a MakeCredentialResponse to the client.
  async makeCredential(options) {
    var response = null;
    if (this.status_ == blink.mojom.AuthenticatorStatus.SUCCESS) {
      let info = { id: this.id_,
            authenticatorData: this.authenticatorData_,
            rawId: this.rawId_,
            clientDataJson: this.clientDataJson_,
          };
      response = { info: info,
            attestationObject: this.attestationObject_,
            transports: [blink.mojom.AuthenticatorTransport.INTERNAL],
            echoHmacCreateSecret: false,
            hmacCreateSecret: false,
            publicKeyAlgo: 0,
          };
    }
    let status = this.status_;
    this.reset();
    return {status, credential: response};
  }

  async getAssertion(options) {
    var response = null;
  if (this.status_ == blink.mojom.AuthenticatorStatus.SUCCESS) {
      let info = { id: this.id_,
            authenticatorData: this.authenticatorData_,
            rawId: this.rawId_,
            clientDataJson: this.clientDataJson_,
          };
      response = { info: info,
            signature: this.signature_,
            userHandle: this.userHandle_,
            echoAppidExtension: false,
            appidExtension: false,
          };
    }
    let status = this.status_;
    this.reset();
    return {status, credential: response};
  }

  async isUserVerifyingPlatformAuthenticatorAvailable() {
    return false;
  }

  async cancel() {}

  // Resets state of mock Authenticator.
  reset() {
    this.status_ = blink.mojom.AuthenticatorStatus.UNKNOWN_ERROR;
    this.id_ = null;
    this.rawId_ = new Uint8Array(0);
    this.clientDataJson_ = new Uint8Array(0);
    this.attestationObject_ = new Uint8Array(0);
    this.authenticatorData_ = new Uint8Array(0);
    this.signature_ = new Uint8Array(0);
    this.userHandle_ = new Uint8Array(0);
  }

  // Sets everything needed for a MakeCredential success response.
  setDefaultsForSuccessfulMakeCredential() {
    mockAuthenticator.setRawId(RAW_ID);
    mockAuthenticator.setId(ID);
    mockAuthenticator.setClientDataJson(CLIENT_DATA_JSON);
    mockAuthenticator.setAttestationObject(ATTESTATION_OBJECT);
    mockAuthenticator.setAuthenticatorStatus(
        blink.mojom.AuthenticatorStatus.SUCCESS);
  }

  // Sets everything needed for a GetAssertion success response.
  setDefaultsForSuccessfulGetAssertion() {
    mockAuthenticator.setRawId(RAW_ID);
    mockAuthenticator.setId(ID);
    mockAuthenticator.setClientDataJson(CLIENT_DATA_JSON);
    mockAuthenticator.setAuthenticatorData(AUTHENTICATOR_DATA);
    mockAuthenticator.setSignature(SIGNATURE);
    mockAuthenticator.setAuthenticatorStatus(
        blink.mojom.AuthenticatorStatus.SUCCESS);
  }

  setAuthenticatorStatus(status) {
    this.status_ = status;
  }

  setId(id) {
    this.id_ = id;
  }

  setRawId(rawId) {
    this.rawId_ = rawId;
  }

  setClientDataJson(clientDataJson) {
    this.clientDataJson_ = clientDataJson;
  }

  setAttestationObject(attestationObject) {
    this.attestationObject_ = attestationObject;
  }

  setAuthenticatorData(authenticatorData) {
    this.authenticatorData_ = authenticatorData;
  }

  setSignature(signature) {
    this.signature_ = signature;
  }

  setUserHandle(userHandle) {
    this.userHandle_ = userHandle;
  }
}

// Mocks the WebOTPService interface defined in webotp_service.mojom.
class MockWebOTPService {
  constructor() {
    this.reset();

    this.interceptor_ = new MojoInterfaceInterceptor(
        blink.mojom.WebOTPService.$interfaceName, 'context', true);
    this.interceptor_.oninterfacerequest = (e) => {
      this.bindHandleToReceiver(e.handle);
    };
    this.interceptor_.start();
  }

  bindHandleToReceiver(handle) {
    this.receiver_ = new blink.mojom.WebOTPServiceReceiver(this);
    this.receiver_.$.bindHandle(handle);
  }

  // Mock functions:
  async receive() {
    return {status: this.status_, otp: this.otp_};
  }

  async abort() {}

  // Resets state of mock WebOTPService.
  reset() {
    this.otp_ = '';
    this.status_ = blink.mojom.SmsStatus.kTimeout;
  }

  setOtp(otp) {
    this.otp_ = otp;
  }

  setStatus(status) {
    this.status_ = status;
  }
}

var mockAuthenticator = new MockAuthenticator();
var mockCredentialManager = new MockCredentialManager();
var mockWebOTPService = new MockWebOTPService();
