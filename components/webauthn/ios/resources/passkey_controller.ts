// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Enables passkey-related interactions between the browser and
 * the renderer by shimming the `navigator.credentials` API.
 */

import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {generateRandomId, sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

// Must be kept in sync with passkey_java_script_feature.mm.
const HANDLER_NAME = 'PasskeyInteractionHandler';

// AAGUID value of Google Password Manager.
const GPM_AAGUID = new Uint8Array([
  // clang-format off
  0xea, 0x9b, 0x8d, 0x66, 0x4d, 0x01, 0x1d, 0x21,
  0x3c, 0xe4, 0xb6, 0xb4, 0x8c, 0xb5, 0x75, 0xd4,
  // clang-format on
]);

// The algorithm supported for passkey registration or assertion.
const ES256 = -7;

// Checks whether provided aaguid is equal to Google Password Manager's aaguid.
function isGpmAaguid(aaguid: Uint8Array): boolean {
  if (aaguid.byteLength !== GPM_AAGUID.byteLength) {
    return false;
  }
  for (let i = 0; i < aaguid.byteLength; i++) {
    if (aaguid[i] !== GPM_AAGUID[i]) {
      return false;
    }
  }
  return true;
}

/**
 * Caches the existing value of WebKit's navigator.credentials, so that the
 * calls can be forwarded to it, if needed.
 */
const cachedNavigatorCredentials: CredentialsContainer = navigator.credentials;

// A function will be defined here by the placeholder replacement.
// It will be called to determine whether to attempt to handle modal
// passkeys requests directly in the browser.
declare const shouldHandleModalPasskeyRequests: () => boolean;

// A function will be defined here by the placeholder replacement.
// It will be called to determine whether to attempt to handle conditional
// passkeys requests directly in the browser.
declare const shouldHandleConditionalPasskeyRequests: () => boolean;

// A function will be defined here by the placeholder replacement.
// It will be called to determine whether to shim
// PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable.
declare const shouldShimIsUVPAA: () => boolean;

/*! {{PLACEHOLDER_HANDLE_PASSKEY_REQUESTS}} */

// Returns whether a passkey request uses conditional mediation.
function isConditionalMediation(
    options: CredentialRequestOptions|CredentialCreationOptions): boolean {
  return ('mediation' in options && options.mediation === 'conditional');
}

// Returns whether passkey requests should be handled directly in the browser.
function shouldHandlePasskeyRequests(isConditional: boolean): boolean {
  return isConditional ? shouldHandleConditionalPasskeyRequests() :
                         shouldHandleModalPasskeyRequests();
}

// Partial interface for overriding getClientCapabilities properties.
interface PublicKeyCredentialClientCapabilities {
  // Whether conditional passkey requests are supported.
  conditionalGet?: boolean;
  conditionalCreate?: boolean;

  // Whether the platform supports user-verifying platform authenticators.
  userVerifyingPlatformAuthenticator?: boolean;
}

// Class to backup and override PublicKeyCredential methods.
class PublicKeyCredentialOverrider {
  private static readonly IS_UVPAA =
      'isUserVerifyingPlatformAuthenticatorAvailable';

  // PublicKeyCredential.isConditionalMediationAvailable.
  private originalIsConditionalMediationAvailable:
      (() => Promise<boolean>)|undefined;

  // PublicKeyCredential.getClientCapabilities.
  private originalGetClientCapabilities:
      (() => Promise<PublicKeyCredentialClientCapabilities>)|undefined;

  // PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable.
  private originalIsUVPAA: (() => Promise<boolean>)|undefined;

  constructor() {
    // Backup methods which may get overridden.
    // TODO(crbug.com/483522384): PublicKeyCredential is sometimes undefined,
    // ensure this workaround is sufficient.
    if (typeof PublicKeyCredential !== 'undefined') {
      if (PublicKeyCredential.isConditionalMediationAvailable) {
        this.originalIsConditionalMediationAvailable =
            PublicKeyCredential.isConditionalMediationAvailable.bind(
                PublicKeyCredential);
      }

      if (PublicKeyCredential.getClientCapabilities) {
        this.originalGetClientCapabilities =
            PublicKeyCredential.getClientCapabilities.bind(PublicKeyCredential);
      }

      if (PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable) {
        this.originalIsUVPAA =
            PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable
                .bind(PublicKeyCredential);
      }
    }
  }

  // Overrides PublicKeyCredential methods to expose the browser's capabilities.
  override(): void {
    // Only override PublicKeyCredential's behaviour when the browser is
    // handling passkey requests.
    if (shouldHandleConditionalPasskeyRequests() ||
        shouldHandleModalPasskeyRequests()) {
      // While conditional passkey requests are handled by the browser, force
      // enable conditional mediation.
      if (shouldHandleConditionalPasskeyRequests()) {
        this.overrideToTrue('isConditionalMediationAvailable');
      }

      // While passkey requests are handled by the browser, a platform
      // authenticator is available.
      this.overrideToTrue(PublicKeyCredentialOverrider.IS_UVPAA);

      // Match the behaviour of getClientCapabilities to the overridden
      // functions above.
      Object.defineProperty(PublicKeyCredential, 'getClientCapabilities', {
        value: async () => {
          let capabilities: PublicKeyCredentialClientCapabilities = {};

          if (this.originalGetClientCapabilities) {
            capabilities = await this.originalGetClientCapabilities();
          }

          if (shouldHandleConditionalPasskeyRequests()) {
            capabilities.conditionalGet = true;
            capabilities.conditionalCreate = true;
          }
          capabilities.userVerifyingPlatformAuthenticator = true;
          return capabilities;
        },
      });
    } else if (shouldShimIsUVPAA()) {
      Object.defineProperty(
          PublicKeyCredential, PublicKeyCredentialOverrider.IS_UVPAA, {
            value: () => this.isUvpaaShim(),
          });
    }
  }

  // Forces a PublicKeyCredential method to always return true.
  private overrideToTrue(methodName: string): void {
    Object.defineProperty(PublicKeyCredential, methodName, {
      value: () => Promise.resolve(true),
    });
  }

  // A replacement for
  // PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable. This
  // is a temporary workaround for the fact that certain WebKit versions
  // incorrectly always return false for this method.
  // See crbug.com/465915379.
  private async isUvpaaShim(): Promise<boolean> {
    if (this.originalIsUVPAA) {
      const value = await this.originalIsUVPAA();

      // Try the cached version first, and short-circuit if it returns true.
      // Our workaround is only targeting false negatives.
      if (value) {
        return true;
      }
    }

    // WebKit's `getClientCapabilities` is not affected by the same bug,
    // and the value returned for that object's
    // 'userVerifyingPlatformAuthenticator' property should always match
    // `isUserVerifyingPlatformAuthenticatorAvailable`. Get and return that
    // value instead.
    if (this.originalGetClientCapabilities) {
      return (await this.originalGetClientCapabilities())
                 ?.userVerifyingPlatformAuthenticator ??
          false;
    }

    return false;
  }

  // Checks whether the conditional get or conditional create capability was
  // originally supported by the renderer.
  private async checkOriginalCapability(
      key: 'conditionalGet'|'conditionalCreate'): Promise<boolean> {
    if (this.originalIsConditionalMediationAvailable) {
      const isAvailable = await this.originalIsConditionalMediationAvailable();
      if (isAvailable) {
        return true;
      }
    }

    if (this.originalGetClientCapabilities) {
      const capabilities = await this.originalGetClientCapabilities();
      if (capabilities[key]) {
        return true;
      }
    }

    return false;
  }

  // Returns whether conditional get was originally supported.
  async checkOriginalConditionalGetCapability(): Promise<boolean> {
    return this.checkOriginalCapability('conditionalGet');
  }

  // Returns whether conditional create was originally supported.
  async checkOriginalConditionalCreateCapability(): Promise<boolean> {
    return this.checkOriginalCapability('conditionalCreate');
  }
}

const publicKeyCredentialOverrider = new PublicKeyCredentialOverrider();

// Returns whether a Credential is a PublicKeyCredential upon successful
// completion of navigator.credentials.create() or navigator.credentials.get().
function isPublicKeyCredential(credential: Credential):
    credential is PublicKeyCredential {
  // Verify that the Credential interface matches the webauthn spec as described
  // here: https://w3c.github.io/webappsec-credential-management/#credential
  if (credential.type !== 'public-key' || typeof credential.id !== 'string') {
    return false;
  }

  // Verify that the PublicKeyCredential interface matches the webauthn spec as
  // described here: https://w3c.github.io/webauthn/#iface-pkcredential
  // Note that, while `authenticatorAttachment` is nullable, this attribute
  // reports the authenticator attachment modality in effect at the time the
  // navigator.credentials.create() or navigator.credentials.get() methods
  // successfully complete, so it should not be null.
  const publicKeyCredential: PublicKeyCredential =
      (credential as PublicKeyCredential);
  return publicKeyCredential.rawId instanceof ArrayBuffer &&
      typeof publicKeyCredential.authenticatorAttachment === 'string' &&
      publicKeyCredential.response.clientDataJSON instanceof ArrayBuffer &&
      typeof (publicKeyCredential.getClientExtensionResults) === 'function' &&
      typeof (publicKeyCredential.toJSON) === 'function';
}

// Converts an array buffer to a base 64 encoded (forgiving policy) string.
// base64 spec: https://datatracker.ietf.org/doc/html/rfc4648#autoid-9
function arrayBufferToBase64(buffer: ArrayBufferLike): string {
  // TODO(crbug.com/460485333): replace with binary.toBase64() on WebKit 18.2+.
  const binary = new Uint8Array(buffer);
  let base64 = '';
  binary.forEach((byte) => {
    base64 += String.fromCharCode(byte);
  });

  return btoa(base64);
}

// Converts an array buffer to a base 64 encoded (strict policy) string.
// base64url spec: https://datatracker.ietf.org/doc/html/rfc4648#autoid-10
function arrayBufferToBase64URL(buffer: ArrayBufferLike): string {
  // URL-Safe substitutions.
  let urlSafeBase64 = arrayBufferToBase64(buffer)
                          .replace(/\+/g, '-')   // Replace + with -
                          .replace(/\//g, '_');  // Replace / with _

  // Remove padding characters (=).
  urlSafeBase64 = urlSafeBase64.replace(/={1,2}$/, '');

  return urlSafeBase64;
}

// Converts a string to array buffer.
function stringToArrayBuffer(str: string): ArrayBuffer {
  const len = str.length;
  const bytes = new Uint8Array(len);

  for (let i = 0; i < len; i++) {
    bytes[i] = str.charCodeAt(i);
  }

  return bytes.buffer;
}

// Converts a base 64 encoded string (strict policy) to an array buffer.
// base64url spec: https://datatracker.ietf.org/doc/html/rfc4648#autoid-10
function decodeBase64URLToArrayBuffer(base64: string): ArrayBuffer {
  // Reverse URL-Safe substitutions.
  let standardBase64 = base64
                           .replace(/-/g, '+')   // Replace - with +
                           .replace(/_/g, '/');  // Replace _ with /

  // Re-add padding characters (=).
  const paddingLength = (4 - standardBase64.length % 4) % 4;
  standardBase64 += '==='.substring(0, paddingLength);

  return stringToArrayBuffer(atob(standardBase64));
}

// Converts a buffer source to a base 64 URL encoded string.
function bufferSourceToBase64URL(buffer: BufferSource): string {
  return arrayBufferToBase64URL(
      buffer instanceof ArrayBuffer ? buffer : buffer.buffer);
}

// Options type containing both types of public key credential options.
type Options =
    PublicKeyCredentialCreationOptions|PublicKeyCredentialRequestOptions;

// Checks if the object is a PublicKeyCredentialCreationOptions.
function isCreationOptions(options: Options):
    options is PublicKeyCredentialCreationOptions {
  // Creation options have the 'user' field, Request options do not.
  return (options as PublicKeyCredentialCreationOptions).user !== undefined;
}

// Interface containing all user related information.
interface UserEntity {
  id: string;
  name: string;
  displayName: string;
}

// Returns a dictionary of the user's entity.
function extractUserEntity(user: PublicKeyCredentialUserEntity): UserEntity {
  return {
    'id': bufferSourceToBase64URL(user.id),
    'name': user.name,
    'displayName': user.displayName,
  };
}

// Interface containing all relying party related information.
interface RelyingPartyEntity {
  id: string;
  name?: string;
}

// Returns a dictionary of the relying party's entity.
function extractRelyingPartyEntity(options: Options): RelyingPartyEntity {
  if (isCreationOptions(options)) {
    return {
      'id': options.rp.id ?? document.location.host,
      'name': options.rp.name,
    };
  } else {  // PublicKeyCredentialRequestOptions
    return {
      'id': options.rpId ?? document.location.host,
    };
  }
}

// Interface containing information about the request.
interface RequestInformation {
  challenge: string;
  userVerification: string;
  isConditional: boolean;
  extensions: AuthenticationExtensionsClientInputs|undefined;
}

// Returns a dictionary of this request's information.
function extractRequestInformation(
    options: Options, isConditional: boolean): RequestInformation {
  let uvRequirement: UserVerificationRequirement|undefined;
  if (isCreationOptions(options)) {
    uvRequirement = options.authenticatorSelection?.userVerification;
  } else {  // PublicKeyCredentialRequestOptions
    uvRequirement = options.userVerification;
  }

  return {
    'challenge': bufferSourceToBase64URL(options.challenge),
    'userVerification': uvRequirement ?? 'unknown',
    'isConditional': isConditional,
    'extensions': options.extensions,
  };
}

// Utility function to ensure transports are expressed as an array of strings.
function transportsAsStrings(transports?: AuthenticatorTransport[]): string[] {
  return (transports ?? []).map(transport => transport as string);
}

// Interface containing information about the credential descriptors.
interface SerializedDescriptor {
  type: string;
  id: string;
  transports: string[];
}

// Serializes a PublicKeyCredentialDescriptor array to a serialized descriptors
// array.
function publicKeyCredentialDescriptorAsSerializedDescriptors(
    descriptors?: PublicKeyCredentialDescriptor[]): SerializedDescriptor[] {
  if (!descriptors) {
    return [];
  }

  // Map the array and convert BufferSource to base64.
  return descriptors.map((desc) => ({
                           type: desc.type,
                           id: bufferSourceToBase64URL(desc.id),
                           transports: transportsAsStrings(desc.transports),
                         }));
}

// Converts PRF values to strings (Base64URL).
function prfValuesToBase64URL(values: AuthenticationExtensionsPRFValues):
    AuthenticationExtensionsPRFValuesJSON {
  const result: AuthenticationExtensionsPRFValuesJSON = {
    first: bufferSourceToBase64URL(values.first),
  };
  if (values.second) {
    result.second = bufferSourceToBase64URL(values.second);
  }
  return result;
}

// Serializes all PRF-related data from the extensions dictionary.
function serializePRF(prf: AuthenticationExtensionsPRFInputs):
    AuthenticationExtensionsPRFInputsJSON {
  const result: AuthenticationExtensionsPRFInputsJSON = {};

  // Add main PRF values to result as Base64 strings if present.
  if (prf.eval) {
    result.eval = prfValuesToBase64URL(prf.eval);
  }

  // Get per credential PRF values as Base64 strings if present.
  const perCredentialPRFData:
      Map<string, AuthenticationExtensionsPRFValuesJSON> = new Map();
  for (const credentialId in prf.evalByCredential) {
    const credentialPRFData = prf.evalByCredential[credentialId];
    if (credentialPRFData) {
      // credentialId is base64url encoded, as specified by the webauthn spec
      // here: https://www.w3.org/TR/webauthn-3/#prf-extension
      perCredentialPRFData.set(
          credentialId, prfValuesToBase64URL(credentialPRFData));
    }
  }

  // Copy per credential PRF values as Base64 strings to result if present.
  if (prf.evalByCredential) {
    result.evalByCredential =
        Object.fromEntries(perCredentialPRFData.entries());
  }

  return result;
}

// Serialize all extension inputs.
function serializeExtensions(extensions?: AuthenticationExtensionsClientInputs):
    AuthenticationExtensionsClientInputsJSON {
  const result: AuthenticationExtensionsClientInputsJSON = {};

  if (!extensions) {
    return result;
  }

  if (extensions.prf) {
    result.prf = serializePRF(extensions.prf);
  }

  // TODO(crbug.com/460485679): Support extensions other than PRF.

  return result;
}

// Converts Base64URL strings to PRF values.
function prfBase64URLToValues(outputs: AuthenticationExtensionsPRFValuesJSON):
    AuthenticationExtensionsPRFValues {
  const result: AuthenticationExtensionsPRFValues = {
    first: decodeBase64URLToArrayBuffer(outputs.first),
  };
  if (outputs.second) {
    result.second = decodeBase64URLToArrayBuffer(outputs.second);
  }
  return result;
}

// Interface containing serialized PRF outputs.
// eslint-disable-next-line @typescript-eslint/naming-convention
interface AuthenticationExtensionsPRFOutputsJSON {
  enabled: boolean;
  results: AuthenticationExtensionsPRFValuesJSON;
}

// Interface containing serialized extension outputs.
// eslint-disable-next-line @typescript-eslint/naming-convention
interface AuthenticationExtensionsClientOutputsJSON {
  prf: AuthenticationExtensionsPRFOutputsJSON;
}

// Deserializes all PRF-related data from the extensions dictionary.
function deserializePRF(prf: AuthenticationExtensionsPRFOutputsJSON):
    AuthenticationExtensionsPRFOutputs {
  return {enabled: prf.enabled, results: prfBase64URLToValues(prf.results)};
}

// Deserialize all extension outputs.
function deserializeExtensions(
    extensions?: AuthenticationExtensionsClientOutputsJSON):
    AuthenticationExtensionsClientOutputs {
  const result: AuthenticationExtensionsClientOutputs = {};

  if (!extensions) {
    return result;
  }

  if (extensions.prf) {
    result.prf = deserializePRF(extensions.prf);
  }

  // TODO(crbug.com/460485679): Support extensions other than PRF.

  return result;
}

// Creates a PublicKeyCredential from the provided list of arguments.
// The credential's type is always set to 'public-key'.
function createPublicKeyCredential(
    authenticatorAttachment: string, rawId: ArrayBuffer,
    response: AuthenticatorResponse,
    extensionOutputs: AuthenticationExtensionsClientOutputs):
    PublicKeyCredential {
  return {
    id: arrayBufferToBase64URL(rawId),
    type: 'public-key',
    authenticatorAttachment: authenticatorAttachment,
    rawId: rawId,
    response: response,
    getClientExtensionResults(): AuthenticationExtensionsClientOutputs {
      return extensionOutputs;
    },
    toJSON(): Record<string, unknown> {
      return {
        id: this.id,
        type: this.type,
        authenticatorAttachment: this.authenticatorAttachment,
        rawId: this.rawId,
        response: this.response,
      };
    },
  };
}

// Creates an empty credential, which will be used to resolve a Credential
// promise so that the promise resolution is deferred to the renderer.
function createEmptyCredential(): PublicKeyCredential {
  const emptyArray = new ArrayBuffer(0);
  const emptyResponse: AuthenticatorResponse = {clientDataJSON: emptyArray};
  return createPublicKeyCredential('', emptyArray, emptyResponse, {});
}

// Returns whether a credential is non empty.
function isValidCredential(credential: Credential|null): boolean {
  return !!credential && !!credential.type && !!credential.id &&
      isPublicKeyCredential(credential) &&
      !!credential.authenticatorAttachment && !!credential.rawId &&
      !!credential.response && !!credential.response.clientDataJSON;
}

// Creates a valid AuthenticatorAttestationResponse from the provided list of
// arguments, as specified by the webauthn spec here:
// https://www.w3.org/TR/webauthn-2/#authenticatorattestationresponse
function createAuthenticatorAttestationResponse(
    attestationObj: ArrayBuffer, authenticatorData: ArrayBuffer,
    publicKeySpkiDer: ArrayBuffer,
    clientDataJson: string): AuthenticatorAttestationResponse {
  return {
    attestationObject: attestationObj,
    clientDataJSON: stringToArrayBuffer(clientDataJson),
    getAuthenticatorData(): ArrayBuffer {
      return authenticatorData;
    },
    getPublicKey(): ArrayBuffer |
        null {
          return publicKeySpkiDer;
        },
    getPublicKeyAlgorithm(): number {
      return ES256;
    },
    getTransports(): string[] {
      // Passkeys created by GPM have the 'hybrid' and 'internal' transports.
      return ['hybrid', 'internal'];
    },
  };
}

// Resolve and reject functions types used by the deferred promise.
type ResolveFunction<T> = (value: T|PromiseLike<T>) => void;
type RejectFunction = (reason?: any) => void;

// Class containing a promise and access to its resolve and reject method for
// later use.
class DeferredPublicKeyCredentialPromise {
  // eslint-disable-next-line @typescript-eslint/explicit-member-accessibility
  public promise: Promise<PublicKeyCredential>;
  // Resolve function of the deferred promise.
  private resolve!: ResolveFunction<PublicKeyCredential>;
  // Reject function of the deferred promise.
  private reject!: RejectFunction;
  // Unique ID for this deferred promise.
  // eslint-disable-next-line @typescript-eslint/explicit-member-accessibility
  public readonly id: string;
  // Map of deferred promises with unique IDs.
  private static ongoingPromises:
      Map<string, DeferredPublicKeyCredentialPromise> = new Map();

  constructor(timeoutMs?: number) {
    this.promise = Promise.race([
      new Promise<PublicKeyCredential>((resolve, reject) => {
        this.resolve = (value) => {
          resolve(value);
          DeferredPublicKeyCredentialPromise.ongoingPromises.delete(this.id);
        };

        this.reject = (reason) => {
          reject(reason);
          DeferredPublicKeyCredentialPromise.ongoingPromises.delete(this.id);
        };
      }),
      new Promise<PublicKeyCredential>((_, reject) => {
        setTimeout(() => {
          reject(new Error(`Promise timed out after ${timeoutMs}ms`));
        }, timeoutMs);
      }),
    ]);

    // Assign a unique ID for this object.
    this.id = generateRandomId();

    DeferredPublicKeyCredentialPromise.ongoingPromises.set(this.id, this);
  }

  // Resolves a deferred promise using the provided credential.
  static resolve(id: string, cred: PublicKeyCredential): void {
    DeferredPublicKeyCredentialPromise.ongoingPromises.get(id)?.resolve(cred);
  }

  // Rejects a deferred promise.
  static reject(id: string): void {
    DeferredPublicKeyCredentialPromise.ongoingPromises.get(id)?.reject();
  }
}

// Creates a passthrough registration request from the provided parameters.
// The passthrough request invokes the WebKit implementation of
// `navigator.credentials.get()` and, upon completion, informs the browser for
// metrics purposes.
function createPassthroughRegistrationRequest(
    options?: CredentialCreationOptions|undefined): Promise<Credential|null> {
  sendWebKitMessage(HANDLER_NAME, {'event': 'logCreateRequest'});

  return cachedNavigatorCredentials.create(options).then((credential) => {
    if (credential && isPublicKeyCredential(credential)) {
      const response: AuthenticatorAttestationResponse =
          credential.response as AuthenticatorAttestationResponse;
      // Parse the aaguid from authenticator data according to
      // https://w3c.github.io/webauthn/#sctn-authenticator-data.
      const aaguid = new Uint8Array(
          response.getAuthenticatorData().slice(37).slice(0, 16));
      sendWebKitMessage(HANDLER_NAME, {
        'event': 'logCreateResolved',
        'isGpm': isGpmAaguid(aaguid),
      });
    }
    return credential;
  });
}

// Creates a passthrough assertion request from the provided parameters.
// The passthrough request invokes the WebKit implementation of
// `navigator.credentials.get()` and, upon completion, informs the browser for
// metrics purposes.
function createPassthroughAssertionRequest(
    options?: CredentialRequestOptions|undefined): Promise<Credential|null> {
  sendWebKitMessage(HANDLER_NAME, {'event': 'logGetRequest'});

  return cachedNavigatorCredentials.get(options).then((credential) => {
    if (credential && isPublicKeyCredential(credential)) {
      // rpId is an optional member of publicKey. Default value (caller's
      // origin domain) should be used if it is not specified
      // (https://w3c.github.io/webauthn/#dom-publickeycredentialrequestoptions-rpid).
      const rpId = options!.publicKey!.rpId ?? document.location.host;
      sendWebKitMessage(HANDLER_NAME, {
        'event': 'logGetResolved',
        'credentialId': credential.id,
        'rpId': rpId,
      });
    }
    return credential;
  });
}

// Creates a registration request from the provided parameters.
function createRegistrationRequest(
    publicKeyOptions: PublicKeyCredentialCreationOptions,
    isConditional: boolean): Promise<Credential|null> {
  const deferredPromise =
      new DeferredPublicKeyCredentialPromise(publicKeyOptions.timeout);

  sendWebKitMessage(HANDLER_NAME, {
    'event': 'handleCreateRequest',
    'frameId': gCrWeb.getFrameId(),
    'requestId': deferredPromise.id,
    'request': extractRequestInformation(publicKeyOptions, isConditional),
    'rpEntity': extractRelyingPartyEntity(publicKeyOptions),
    'userEntity': extractUserEntity(publicKeyOptions.user),
    'excludeCredentials': publicKeyCredentialDescriptorAsSerializedDescriptors(
        publicKeyOptions.excludeCredentials),
    'extensions': serializeExtensions(publicKeyOptions.extensions),
  });  // Attestation request

  return deferredPromise.promise;
}

// Creates an assertion request from the provided parameters.
function createAssertionRequest(
    publicKeyOptions: PublicKeyCredentialRequestOptions,
    isConditional: boolean): Promise<Credential|null> {
  const deferredPromise =
      new DeferredPublicKeyCredentialPromise(publicKeyOptions.timeout);

  sendWebKitMessage(HANDLER_NAME, {
    'event': 'handleGetRequest',
    'frameId': gCrWeb.getFrameId(),
    'requestId': deferredPromise.id,
    'request': extractRequestInformation(publicKeyOptions, isConditional),
    'rpEntity': extractRelyingPartyEntity(publicKeyOptions),
    'allowCredentials': publicKeyCredentialDescriptorAsSerializedDescriptors(
        publicKeyOptions.allowCredentials),
    'extensions': serializeExtensions(publicKeyOptions.extensions),
  });  // Assertion request

  return deferredPromise.promise;
}

/**
 * Chromium-specific implementation of CredentialsContainer.
 */
const credentialsContainer: CredentialsContainer = {
  get: function(options?: CredentialRequestOptions): Promise<Credential|null> {
    // Only process WebAuthn requests.
    if (!options?.publicKey) {
      return cachedNavigatorCredentials.get(options);
    }

    const isConditional: boolean = isConditionalMediation(options);
    if (shouldHandlePasskeyRequests(isConditional) &&
        options.publicKey.challenge) {
      return createAssertionRequest(options.publicKey, isConditional)
          .then(result => {
            if (isValidCredential(result)) {
              // TODO(crbug.com/460485333): Notification message of success
              // here?
              return result;
            }

            return createPassthroughAssertionRequest(options);
          });
    } else {
      return createPassthroughAssertionRequest(options);
    }
  },
  create: function(
      options?: CredentialCreationOptions|undefined): Promise<Credential|null> {
    // Only process WebAuthn requests.
    if (!options?.publicKey) {
      return cachedNavigatorCredentials.create(options);
    }

    const isConditional: boolean = isConditionalMediation(options);
    if (shouldHandlePasskeyRequests(isConditional) &&
        options.publicKey.challenge && options.publicKey.user &&
        options.publicKey.user.id) {
      return createRegistrationRequest(options.publicKey, isConditional)
          .then(result => {
            if (isValidCredential(result)) {
              // TODO(crbug.com/460485333): Notification message of success
              // here?
              return result;
            }

            return createPassthroughRegistrationRequest(options);
          });
    } else {
      return createPassthroughRegistrationRequest(options);
    }
  },
  preventSilentAccess: function(): Promise<any> {
    return cachedNavigatorCredentials.preventSilentAccess();
  },
  store: function(credentials?: any): Promise<any> {
    return cachedNavigatorCredentials.store(credentials);
  },
};

// Override the existing value of `navigator.credentials` with our own. The use
// of Object.defineProperty (versus just doing `navigator.credentials = ...`) is
// a workaround for the fact that `navigator.credentials` is readonly.
Object.defineProperty(navigator, 'credentials', {value: credentialsContainer});

// Function called from C++ to yield the passkey request back to the OS.
function deferToRenderer(requestId: string, requestType: number): void {
  // LINT.IfChange
  // Whether the request in modal, conditional get or conditional create.
  enum RequestType {
    // Unknown (due to bad request).
    UNKNOWN,
    // Modal (non conditional) request.
    MODAL,
    // Conditional assertion request.
    CONDITIONAL_GET,
    // Conditional registration request.
    CONDITIONAL_CREATE,
  }
  // LINT.ThenChange(//components/webauthn/ios/passkey_request_params.h)

  const emptyCredential: PublicKeyCredential = createEmptyCredential();

  if (requestType === RequestType.CONDITIONAL_GET) {
    publicKeyCredentialOverrider.checkOriginalConditionalGetCapability().then(
        (isAvailable) => {
          if (isAvailable) {
            DeferredPublicKeyCredentialPromise.resolve(
                requestId, emptyCredential);
          } else {
            DeferredPublicKeyCredentialPromise.reject(requestId);
          }
        });
  } else if (requestType === RequestType.CONDITIONAL_CREATE) {
    publicKeyCredentialOverrider.checkOriginalConditionalCreateCapability()
        .then((isAvailable) => {
          if (isAvailable) {
            DeferredPublicKeyCredentialPromise.resolve(
                requestId, emptyCredential);
          } else {
            DeferredPublicKeyCredentialPromise.reject(requestId);
          }
        });
  } else {  // MODAL or UNKNOWN
    DeferredPublicKeyCredentialPromise.resolve(requestId, emptyCredential);
  }
}

// Function called from C++ to reject a passkey request.
function rejectPasskeyRequest(requestId: string): void {
  DeferredPublicKeyCredentialPromise.reject(requestId);
}


// Resolves the credential promise with the provided response.
function resolveCredentialPromise(
    requestId: string, id64: string, response: AuthenticatorResponse,
    extensions: AuthenticationExtensionsClientOutputsJSON): void {
  const id = decodeBase64URLToArrayBuffer(id64);
  const credential: PublicKeyCredential = createPublicKeyCredential(
      'platform', id, response, deserializeExtensions(extensions));

  DeferredPublicKeyCredentialPromise.resolve(requestId, credential);
}

// Function called from C++ to resolve the deferred promise with a valid
// assertion credential.
function resolveAssertionRequest(
    requestId: string, id64: string, signature64: string,
    authenticatorData64: string, userHandle64: string, clientDataJson: string,
    extensions: AuthenticationExtensionsClientOutputsJSON): void {
  const response: AuthenticatorAssertionResponse = {
    authenticatorData: decodeBase64URLToArrayBuffer(authenticatorData64),
    clientDataJSON: stringToArrayBuffer(clientDataJson),
    signature: decodeBase64URLToArrayBuffer(signature64),
    userHandle: decodeBase64URLToArrayBuffer(userHandle64),
  };

  resolveCredentialPromise(requestId, id64, response, extensions);
}

// Function called from C++ to resolve the deferred promise with a valid
// attestation credential.
function resolveAttestationRequest(
    requestId: string, id64: string, attestationObject64: string,
    authenticatorData64: string, publicKeySpkiDer64: string,
    clientDataJson: string,
    extensions: AuthenticationExtensionsClientOutputsJSON): void {
  const response: AuthenticatorAttestationResponse =
      createAuthenticatorAttestationResponse(
          decodeBase64URLToArrayBuffer(attestationObject64),
          decodeBase64URLToArrayBuffer(authenticatorData64),
          decodeBase64URLToArrayBuffer(publicKeySpkiDer64), clientDataJson);

  resolveCredentialPromise(requestId, id64, response, extensions);
}

const passkey = new CrWebApi('passkey');

passkey.addFunction('deferToRenderer', deferToRenderer);
passkey.addFunction('rejectPasskeyRequest', rejectPasskeyRequest);
passkey.addFunction('resolveAssertionRequest', resolveAssertionRequest);
passkey.addFunction('resolveAttestationRequest', resolveAttestationRequest);

gCrWeb.registerApi(passkey);

// Override PublicKeyCredential's behaviour to expose browser capabilities.
// TODO(crbug.com/483522384): PublicKeyCredential is sometimes undefined, ensure
// this workaround is sufficient.
if (typeof PublicKeyCredential !== 'undefined') {
  publicKeyCredentialOverrider.override();
}
