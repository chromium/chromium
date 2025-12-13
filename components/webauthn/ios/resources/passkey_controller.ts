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

/*! {{PLACEHOLDER_HANDLE_MODAL_PASSKEY_REQUESTS}} */

// Returns whether a passkey request uses conditional mediation.
function isConditionalMediation(
    options: CredentialRequestOptions|CredentialCreationOptions): boolean {
  return ('mediation' in options && options.mediation === 'conditional');
}

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

// Converts a buffer source to a base 64 encoded (forgiving policy) string.
function bufferSourceToBase64(buffer: BufferSource): string {
  return arrayBufferToBase64(
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
    'id': bufferSourceToBase64(user.id),
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
  extensions: AuthenticationExtensionsClientInputs|undefined;
}

// Returns a dictionary of this request's information.
function extractRequestInformation(options: Options): RequestInformation {
  let uvRequirement: UserVerificationRequirement|undefined;
  if (isCreationOptions(options)) {
    uvRequirement = options.authenticatorSelection?.userVerification;
  } else {  // PublicKeyCredentialRequestOptions
    uvRequirement = options.userVerification;
  }

  return {
    'challenge': bufferSourceToBase64(options.challenge),
    'userVerification': uvRequirement ?? 'unknown',
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
                           id: bufferSourceToBase64(desc.id),
                           transports: transportsAsStrings(desc.transports),
                         }));
}

// Creates a PublicKeyCredential from the provided list of arguments.
// The credential's type is always set to 'public-key'.
function createPublicKeyCredential(
    authenticatorAttachment: string, rawId: ArrayBuffer,
    response: AuthenticatorResponse): PublicKeyCredential {
  return {
    id: arrayBufferToBase64URL(rawId),
    type: 'public-key',
    authenticatorAttachment: authenticatorAttachment,
    rawId: rawId,
    response: response,
    getClientExtensionResults(): AuthenticationExtensionsClientOutputs {
      // TODO(crbug.com/460485679): implement when adding extension support.
      return {};
    },
    toJSON(): any {
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
  const nullArray = new ArrayBuffer(0);
  const emptyResponse: AuthenticatorResponse = {clientDataJSON: nullArray};
  return createPublicKeyCredential('', nullArray, emptyResponse);
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
    publicKeyOptions: PublicKeyCredentialCreationOptions):
    Promise<Credential|null> {
  const deferredPromise =
      new DeferredPublicKeyCredentialPromise(publicKeyOptions.timeout);

  sendWebKitMessage(HANDLER_NAME, {
    'event': 'handleCreateRequest',
    'frameId': gCrWeb.getFrameId(),
    'requestId': deferredPromise.id,
    'request': extractRequestInformation(publicKeyOptions),
    'rpEntity': extractRelyingPartyEntity(publicKeyOptions),
    'userEntity': extractUserEntity(publicKeyOptions.user),
    'excludeCredentials': publicKeyCredentialDescriptorAsSerializedDescriptors(
        publicKeyOptions.excludeCredentials),
  });  // Attestation request

  return deferredPromise.promise;
}

// Creates an assertion request from the provided parameters.
function createAssertionRequest(
    publicKeyOptions: PublicKeyCredentialRequestOptions):
    Promise<Credential|null> {
  const deferredPromise =
      new DeferredPublicKeyCredentialPromise(publicKeyOptions.timeout);

  sendWebKitMessage(HANDLER_NAME, {
    'event': 'handleGetRequest',
    'frameId': gCrWeb.getFrameId(),
    'requestId': deferredPromise.id,
    'request': extractRequestInformation(publicKeyOptions),
    'rpEntity': extractRelyingPartyEntity(publicKeyOptions),
    'allowCredentials': publicKeyCredentialDescriptorAsSerializedDescriptors(
        publicKeyOptions.allowCredentials),
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

    if (shouldHandleModalPasskeyRequests() &&
        !isConditionalMediation(options) && options.publicKey.challenge) {
      return createAssertionRequest(options.publicKey).then(result => {
        if (isValidCredential(result)) {
          // TODO(crbug.com/460485333): Notification message of success here?
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

    if (shouldHandleModalPasskeyRequests() &&
        !isConditionalMediation(options) && options.publicKey.challenge &&
        options.publicKey.user && options.publicKey.user.id) {
      return createRegistrationRequest(options.publicKey).then(result => {
        if (isValidCredential(result)) {
          // TODO(crbug.com/460485333): Notification message of success here?
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
function deferToRenderer(requestId: string): void {
  const emptyCredential: PublicKeyCredential = createEmptyCredential();
  DeferredPublicKeyCredentialPromise.resolve(requestId, emptyCredential);
}

// Resolves the credential promise with the provided response.
function resolveCredentialPromise(
    requestId: string, id64: string, response: AuthenticatorResponse): void {
  const id = decodeBase64URLToArrayBuffer(id64);
  const credential: PublicKeyCredential =
      createPublicKeyCredential('platform', id, response);

  DeferredPublicKeyCredentialPromise.resolve(requestId, credential);
}

// Function called from C++ to resolve the deferred promise with a valid
// assertion credential.
function resolveAssertionRequest(
    requestId: string, id64: string, authenticatorData64: string,
    clientDataJson: string, signature64: string, userHandle64: string): void {
  const response: AuthenticatorAssertionResponse = {
    authenticatorData: decodeBase64URLToArrayBuffer(authenticatorData64),
    clientDataJSON: stringToArrayBuffer(clientDataJson),
    signature: decodeBase64URLToArrayBuffer(signature64),
    userHandle: decodeBase64URLToArrayBuffer(userHandle64),
  };

  resolveCredentialPromise(requestId, id64, response);
}

// Function called from C++ to resolve the deferred promise with a valid
// attestation credential.
function resolveAttestationRequest(
    requestId: string, id64: string, attestationObject64: string,
    authenticatorData64: string, publicKeySpkiDer64: string,
    clientDataJson: string): void {
  const response: AuthenticatorAttestationResponse =
      createAuthenticatorAttestationResponse(
          decodeBase64URLToArrayBuffer(attestationObject64),
          decodeBase64URLToArrayBuffer(authenticatorData64),
          decodeBase64URLToArrayBuffer(publicKeySpkiDer64), clientDataJson);

  resolveCredentialPromise(requestId, id64, response);
}

const passkey = new CrWebApi();

passkey.addFunction('deferToRenderer', deferToRenderer);
passkey.addFunction('resolveAssertionRequest', resolveAssertionRequest);
passkey.addFunction('resolveAttestationRequest', resolveAttestationRequest);

gCrWeb.registerApi('passkey', passkey);
