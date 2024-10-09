import {CredentialManager, CredentialManagerError, CredentialManagerReceiver, CredentialType} from '/gen/third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom.m.js';
import {SmsStatus, WebOTPService, WebOTPServiceReceiver} from '/gen/third_party/blink/public/mojom/sms/webotp_service.mojom.m.js';

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

  async report() {
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