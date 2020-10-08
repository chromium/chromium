'use strict';

class VirtualAuthenticator {
  constructor(virtualAuthenticator) {
    this.virtualAuthenticator_ = virtualAuthenticator;
  }

  async uniqueId() {
    let getUniqueIdResponse = await this.virtualAuthenticator_.getUniqueId();
    return getUniqueIdResponse.id;
  }

  // Alias for uniqueId().
  async id() {
    return this.uniqueId();
  }

  async registeredKeys() {
    let getRegistrationsResponse = await this.virtualAuthenticator_.getRegistrations();
    return getRegistrationsResponse.keys;
  }

  async generateAndRegisterKey(keyHandle, rpId) {
    let ecKey = await window.crypto.subtle.generateKey(
        { name: "ECDSA", namedCurve: "P-256" }, true /* extractable */, ["sign", "verify"]);
    let privateKeyPkcs8 = await window.crypto.subtle.exportKey("pkcs8", ecKey.privateKey);
    let registration = {
      privateKey: new Uint8Array(privateKeyPkcs8),
      keyHandle: keyHandle,
      rpId,
      counter: 1,
    };
    let addRegistrationResponse = await this.virtualAuthenticator_.addRegistration(registration);
    return addRegistrationResponse.added;
  }

  async clearRegisteredKeys() {
    let clearRegistrationsResponse = await this.virtualAuthenticator_.clearRegistrations();
    return clearRegistrationsResponse.keys;
  }

  async setLargeBlob(keyHandle, blob) {
    let setLargeBlobResponse = await this.virtualAuthenticator_.setLargeBlob(keyHandle, blob);
    return setLargeBlobResponse.set;
  }

  async getLargeBlob(keyHandle) {
    let getLargeBlobResponse = await this.virtualAuthenticator_.getLargeBlob(keyHandle);
    return getLargeBlobResponse.blob;
  }

  async setUserPresence(present) {
    return this.virtualAuthenticator_.setUserPresence(present);
  }

  async userPresence() {
    let getUserPresenceResponse = await this.virtualAuthenticator_.getUserPresence();
    return getUserPresenceResponse.present;
  }
};

class VirtualAuthenticatorManager {
  constructor() {
    this.virtualAuthenticatorManager_ = new blink.test.mojom.VirtualAuthenticatorManagerRemote;
    Mojo.bindInterface(
        blink.test.mojom.VirtualAuthenticatorManager.$interfaceName,
        this.virtualAuthenticatorManager_.$.bindNewPipeAndPassReceiver()
            .handle);
  }

  async createAuthenticator(options = {}) {
    options = Object.assign(
        {
          protocol: blink.test.mojom.ClientToAuthenticatorProtocol.CTAP2,
          ctap2Version: blink.test.mojom.ClientToAuthenticatorProtocol.CTAP2_0,
          transport: blink.mojom.AuthenticatorTransport.USB,
          attachment: blink.mojom.AuthenticatorAttachment.CROSS_PLATFORM,
          hasResidentKey: true,
          hasUserVerification: true,
          hasLargeBlob: false,
        },
        options);
    let createAuthenticatorResponse =
        await this.virtualAuthenticatorManager_.createAuthenticator(options);
    return new VirtualAuthenticator(createAuthenticatorResponse.authenticator);
  }

  async authenticators() {
    let getAuthenticatorsResponse = await this.virtualAuthenticatorManager_.getAuthenticators();
    let authenticators = [];
    for (let mojo_authenticator of getAuthenticatorsResponse.authenticators) {
      authenticators.push(new VirtualAuthenticator(mojo_authenticator));
    }
    return authenticators;
  }

  async removeAuthenticator(id) {
    let removeAuthenticatorResponse = await this.virtualAuthenticatorManager_.removeAuthenticator(id);
    return removeAuthenticatorResponse.removed;
  }

  async clearAuthenticators(id) {
    return this.virtualAuthenticatorManager_.clearAuthenticators();
  }
};

navigator.credentials.test = new VirtualAuthenticatorManager();
