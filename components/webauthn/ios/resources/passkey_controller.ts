// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Enables passkey-related interactions between the browser and
 * the renderer by shimming the `navigator.credentials` API.
 */

import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

// Must be kept in sync with passkey_java_script_feature.mm.
const HANDLER_NAME = 'PasskeyInteractionHandler';

// AAGUID value of Google Password Manager.
const GPM_AAGUID = new Uint8Array([
  // clang-format off
  0xea, 0x9b, 0x8d, 0x66, 0x4d, 0x01, 0x1d, 0x21,
  0x3c, 0xe4, 0xb6, 0xb4, 0x8c, 0xb5, 0x75, 0xd4,
  // clang-format on
]);

// The algorithm supported for passkey registration or attestation.
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

function createPublicKeyCredential(
    id: string, authenticatorAttachment: string, rawId: ArrayBuffer,
    response: AuthenticatorResponse): PublicKeyCredential {
  return {
    id: id,
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
  // eslint-disable-next-line @typescript-eslint/explicit-member-accessibility
  public resolve!: ResolveFunction<PublicKeyCredential>;
  // eslint-disable-next-line @typescript-eslint/explicit-member-accessibility
  public reject!: RejectFunction;
  // TODO(crbug.com/460485333): keep a map of promises with unique ids instead
  // of a single promise.
  static ongoingPromise: DeferredPublicKeyCredentialPromise|null = null;

  constructor(timeoutMs?: number) {
    this.promise = Promise.race([
      new Promise<PublicKeyCredential>((resolve, reject) => {
        this.resolve = (value) => {
          resolve(value);
          DeferredPublicKeyCredentialPromise.ongoingPromise = null;
        };

        this.reject = (reason) => {
          reject(reason);
          DeferredPublicKeyCredentialPromise.ongoingPromise = null;
        };
      }),
      new Promise<PublicKeyCredential>((_, reject) => {
        setTimeout(() => {
          reject(new Error(`Promise timed out after ${timeoutMs}ms`));
        }, timeoutMs);
      }),
    ]);

    DeferredPublicKeyCredentialPromise.ongoingPromise = this;
  }
}

// Creates a deferred public key credential promise and return its credential
// promise.
function publicKeyCredentialPromise(timeoutMs: number|undefined):
    Promise<Credential|null> {
  return new DeferredPublicKeyCredentialPromise(timeoutMs).promise;
}

// Creates a passthrough registration request from the provided parameters.
// The passthrough request invokes the WebKit implementation of
// `navigator.credentials.get()` and, upon completion, informs the browser for
// metrics purposes.
function createPassthroughRegistrationRequest(
    options?: CredentialCreationOptions|undefined): Promise<Credential|null> {
  sendWebKitMessage(HANDLER_NAME, {'event': 'logCreateRequest'});

  return cachedNavigatorCredentials.create(options).then((credential) => {
    if (credential && credential instanceof PublicKeyCredential &&
        credential.response instanceof AuthenticatorAttestationResponse) {
      // Parse the aaguid from authenticator data according to
      // https://w3c.github.io/webauthn/#sctn-authenticator-data.
      const aaguid = new Uint8Array(
          credential.response.getAuthenticatorData().slice(37).slice(0, 16));
      sendWebKitMessage(HANDLER_NAME, {
        'event': 'logCreateResolved',
        'isGpm': isGpmAaguid(aaguid),
      });
    }
    return credential;
  });
}

// Creates a passthrough attestation request from the provided parameters.
// The passthrough request invokes the WebKit implementation of
// `navigator.credentials.get()` and, upon completion, informs the browser for
// metrics purposes.
function createPassthroughAttestationRequest(
    options?: CredentialRequestOptions|undefined): Promise<Credential|null> {
  sendWebKitMessage(HANDLER_NAME, {'event': 'logGetRequest'});

  return cachedNavigatorCredentials.get(options).then((credential) => {
    if (credential && credential instanceof PublicKeyCredential) {
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
  sendWebKitMessage(HANDLER_NAME, {
    'event': 'handleCreateRequest',
    'frameId': gCrWeb.getFrameId(),
    'request': extractRequestInformation(publicKeyOptions),
    'rpEntity': extractRelyingPartyEntity(publicKeyOptions),
    'userEntity': extractUserEntity(publicKeyOptions.user),
    'excludeCredentials': publicKeyCredentialDescriptorAsSerializedDescriptors(
        publicKeyOptions.excludeCredentials),
  });  // Attestation request

  return publicKeyCredentialPromise(publicKeyOptions.timeout);
}

// Creates an attestation request from the provided parameters.
function createAttestationRequest(
    publicKeyOptions: PublicKeyCredentialRequestOptions):
    Promise<Credential|null> {
  sendWebKitMessage(HANDLER_NAME, {
    'event': 'handleGetRequest',
    'frameId': gCrWeb.getFrameId(),
    'request': extractRequestInformation(publicKeyOptions),
    'rpEntity': extractRelyingPartyEntity(publicKeyOptions),
    'allowCredentials': publicKeyCredentialDescriptorAsSerializedDescriptors(
        publicKeyOptions.allowCredentials),
  });  // Attestation request

  return publicKeyCredentialPromise(publicKeyOptions.timeout);
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
      return createAttestationRequest(options.publicKey).then(_ => {
        // TODO(crbug.com/460485333): validate result and return if valid.
        return createPassthroughAttestationRequest(options);
      });
    } else {
      return createPassthroughAttestationRequest(options);
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
      return createRegistrationRequest(options.publicKey).then(_ => {
        // TODO(crbug.com/460485333): validate result and return if valid.
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
function deferToRenderer(): void {
  const nullArray = new ArrayBuffer(0);
  const emptyResponse: AuthenticatorResponse = {clientDataJSON: nullArray};
  const emptyCredential: PublicKeyCredential =
      createPublicKeyCredential('', '', nullArray, emptyResponse);
  DeferredPublicKeyCredentialPromise.ongoingPromise?.resolve(emptyCredential);
}

// Function called from C++ to resolve the deferred promise with a valid
// attestation credential.
function resolveAttestationRequest(
    id64: string, attestationObject64: string, authenticatorData64: string,
    publicKeySpkiDer64: string, clientDataJson: string): void {
  const attestationObj = decodeBase64URLToArrayBuffer(attestationObject64);
  const authenticatorData = decodeBase64URLToArrayBuffer(authenticatorData64);
  const publicKeySpkiDer = decodeBase64URLToArrayBuffer(publicKeySpkiDer64);
  const response: AuthenticatorAttestationResponse =
      createAuthenticatorAttestationResponse(
          attestationObj, authenticatorData, publicKeySpkiDer, clientDataJson);

  const id = decodeBase64URLToArrayBuffer(id64);
  const credential: PublicKeyCredential = createPublicKeyCredential(
      arrayBufferToBase64URL(id), 'platform', id, response);

  DeferredPublicKeyCredentialPromise.ongoingPromise?.resolve(credential);
}

const passkey = new CrWebApi();

passkey.addFunction('deferToRenderer', deferToRenderer);
passkey.addFunction('resolveAttestationRequest', resolveAttestationRequest);

gCrWeb.registerApi('passkey', passkey);
