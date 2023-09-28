import {CredentialManager, CredentialManagerError, CredentialManagerReceiver, CredentialType} from '/gen/third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom.m.js';
import {SmsStatus, WebOTPService, WebOTPServiceReceiver} from '/gen/third_party/blink/public/mojom/sms/webotp_service.mojom.m.js';
import {Authenticator, AuthenticatorReceiver, AuthenticatorStatus, AuthenticatorTransport} from '/gen/third_party/blink/public/mojom/webauthn/authenticator.mojom.m.js';
import {ATTESTATION_OBJECT, AUTHENTICATOR_DATA, CLIENT_DATA_JSON, ID, RAW_ID, SIGNATURE} from './test-inputs.js';

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
export class MockCredentialManager {
  constructor() {
    this.reset();

    this.interceptor_ =
        new MojoInterfaceInterceptor(CredentialManager.$interfaceName);
    this.interceptor_.oninterfacerequest = e => {
      this.bindHandleToReceiver(e.handle);
    };
    this.interceptor_.start();
  }

  bindHandleToReceiver(handle) {
    this.receiver_ = new CredentialManagerReceiver(this);
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
    if (this.error_ == CredentialManagerError.SUCCESS) {
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
    this.error_ = CredentialManagerError.SUCCESS;
    this.credentialInfo_ =
        this.constructCredentialInfo_(CredentialType.EMPTY, '', '', '', '');
  }

  setResponse(id, password, name, icon) {
    this.credentialInfo_ = this.constructCredentialInfo_(
        CredentialType.PASSWORD, id, password, name, icon);
  }

  setError(error) {
    this.error_ = error;
  }
}

// Class that mocks Authenticator interface defined in authenticator.mojom.
export class MockAuthenticator {
  constructor() {
    this.reset();

    this.interceptor_ =
        new MojoInterfaceInterceptor(Authenticator.$interfaceName);
    this.interceptor_.oninterfacerequest = e => {
      this.bindHandleToReceiver(e.handle);
    };
    this.interceptor_.start();
  }

  bindHandleToReceiver(handle) {
    this.receiver_ = new AuthenticatorReceiver(this);
    this.receiver_.$.bindHandle(handle);
  }

  // Returns a MakeCredentialResponse to the client.
  async makeCredential(options) {
    var response = null;
    if (this.status_ == AuthenticatorStatus.SUCCESS) {
      let info = { id: this.id_,
            authenticatorData: this.authenticatorData_,
            rawId: this.rawId_,
            clientDataJson: this.clientDataJson_,
          };
      response = { info: info,
            attestationObject: this.attestationObject_,
            transports: [AuthenticatorTransport.INTERNAL],
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
    if (this.status_ == AuthenticatorStatus.SUCCESS) {
      let info = { id: this.id_,
            authenticatorData: this.authenticatorData_,
            rawId: this.rawId_,
            clientDataJson: this.clientDataJson_,
          };
      let extensions = { echoAppidExtension: false,
            appidExtension: false,
          };
      response = { info: info,
            signature: this.signature_,
            userHandle: this.userHandle_,
            extensions: extensions,
          };
    }
    let status = this.status_;
    this.reset();
    return {status, credential: response};
  }

  async isUserVerifyingPlatformAuthenticatorAvailable() {
    return false;
  }

  async isConditionalMediationAvailable() {
    return false;
  }

  async cancel() {}

  // Resets state of mock Authenticator.
  reset() {
    this.status_ = AuthenticatorStatus.UNKNOWN_ERROR;
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
    this.setRawId(RAW_ID);
    this.setId(ID);
    this.setClientDataJson(CLIENT_DATA_JSON);
    this.setAttestationObject(ATTESTATION_OBJECT);
    this.setAuthenticatorStatus(AuthenticatorStatus.SUCCESS);
  }

  // Sets everything needed for a GetAssertion success response.
  setDefaultsForSuccessfulGetAssertion() {
    this.setRawId(RAW_ID);
    this.setId(ID);
    this.setClientDataJson(CLIENT_DATA_JSON);
    this.setAuthenticatorData(AUTHENTICATOR_DATA);
    this.setSignature(SIGNATURE);
    this.setAuthenticatorStatus(AuthenticatorStatus.SUCCESS);
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
export class MockWebOTPService {
  constructor() {
    this.reset();

    this.interceptor_ =
        new MojoInterfaceInterceptor(WebOTPService.$interfaceName);
    this.interceptor_.oninterfacerequest = (e) => {
      this.bindHandleToReceiver(e.handle);
    };
    this.interceptor_.start();
  }

  bindHandleToReceiver(handle) {
    this.receiver_ = new WebOTPServiceReceiver(this);
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
    this.status_ = SmsStatus.kTimeout;
  }

  setOtp(otp) {
    this.otp_ = otp;
  }

  setStatus(status) {
    this.status_ = status;
  }
}
