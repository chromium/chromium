import {AuthenticatorAttachment, AuthenticatorTransport} from '/gen/third_party/blink/public/mojom/webauthn/authenticator.mojom.m.js';
import {ClientToAuthenticatorProtocol, VirtualAuthenticatorManager} from '/gen/third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom.m.js';

class VirtualAuthenticator {
  constructor(virtualAuthenticator) {
    this.virtualAuthenticator_ = virtualAuthenticator;
  }

  async uniqueId() {
    const {id} = await this.virtualAuthenticator_.getUniqueId();
    return id;
  }

  // Alias for uniqueId().
  async id() {
    return this.uniqueId();
  }

  async registeredKeys() {
    const {keys} = await this.virtualAuthenticator_.getRegistrations();
    return keys;
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
    const {added} =
        await this.virtualAuthenticator_.addRegistration(registration);
    return added;
  }

  async clearRegisteredKeys() {
    const {keys} = await this.virtualAuthenticator_.clearRegistrations();
    return keys;
  }

  async setLargeBlob(keyHandle, blob) {
    const {set} =
        await this.virtualAuthenticator_.setLargeBlob(keyHandle, blob);
    return set;
  }

  async getLargeBlob(keyHandle) {
    const {blob} = await this.virtualAuthenticator_.getLargeBlob(keyHandle);
    return blob;
  }

  async setUserPresence(present) {
    return this.virtualAuthenticator_.setUserPresence(present);
  }

  async userPresence() {
    const {present} = await this.virtualAuthenticator_.getUserPresence();
    return present;
  }
}

export class TestAuthenticatorManager {
  constructor() {
    this.virtualAuthenticatorManager_ = VirtualAuthenticatorManager.getRemote();
  }

  async createAuthenticator(options = {}) {
    options = Object.assign(
        {
          protocol: ClientToAuthenticatorProtocol.CTAP2,
          ctap2Version: ClientToAuthenticatorProtocol.CTAP2_0,
          transport: AuthenticatorTransport.USB,
          attachment: AuthenticatorAttachment.CROSS_PLATFORM,
          hasResidentKey: true,
          hasUserVerification: true,
          hasLargeBlob: false,
        },
        options);
    const {authenticator} =
        await this.virtualAuthenticatorManager_.createAuthenticator(options);
    return new VirtualAuthenticator(authenticator);
  }

  async authenticators() {
    const {authenticators} =
        await this.virtualAuthenticatorManager_.getAuthenticators();
    return authenticators.map(a => new VirtualAuthenticator(a));
  }

  async removeAuthenticator(id) {
    const {removed} =
        await this.virtualAuthenticatorManager_.removeAuthenticator(id);
    return removed;
  }

  async clearAuthenticators(id) {
    return this.virtualAuthenticatorManager_.clearAuthenticators();
  }
}
