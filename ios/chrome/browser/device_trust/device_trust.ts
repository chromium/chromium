// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  CrWebApi,
  gCrWeb,
} from '//ios/web/public/js_messaging/resources/gcrweb.js';

import {
  sendWebKitMessageWithReply,
  trim,
} from '//ios/web/public/js_messaging/resources/utils.js';

const MESSAGE_HANDLER_NAME = 'DeviceTrustMessageHandler';
const REQUEST_TIMEOUT_MS = 30000;

// LINT.IfChange(MaxChallengeRequestLength)
const MAX_CHALLENGE_REQUEST_LENGTH = 1024;
// LINT.ThenChange(//ios/chrome/browser/device_trust/device_trust_java_script_feature.mm:MaxChallengeRequestLength)
const MAX_CONCURRENT_REQUESTS = 3;

let pendingRequestCount = 0;

interface AttestationReply {
  signedPayload?: unknown;
  errorCode?: unknown;
  errorMessage?: unknown;
}

class DeviceTrustError extends Error {
  constructor(readonly code: string, message: string) {
    super(message);
    this.name = 'DeviceTrustError';
  }
}

function parseAttestationReply(reply: unknown): string {
  if (typeof reply !== 'object' || reply === null) {
    throw new DeviceTrustError('INTERNAL_ERROR', 'Invalid native response.');
  }

  const attestationReply = reply as AttestationReply;
  if (typeof attestationReply.signedPayload === 'string') {
    return attestationReply.signedPayload;
  }

  const errorCode = typeof attestationReply.errorCode === 'string' &&
          attestationReply.errorCode.length > 0 ?
      attestationReply.errorCode :
      'INTERNAL_ERROR';

  const errorMessage = typeof attestationReply.errorMessage === 'string' &&
          attestationReply.errorMessage.length > 0 ?
      attestationReply.errorMessage :
      'Unknown internal error occurred.';

  throw new DeviceTrustError(errorCode, errorMessage);
}

const DeviceTrustAPI = {
  getAttestation: async function(challengeRequest: string): Promise<string> {
    if (typeof challengeRequest !== 'string') {
      throw new DeviceTrustError(
          'INVALID_CHALLENGE_REQUEST', 'challengeRequest must be a string.');
    }

    if (trim(challengeRequest).length === 0) {
      throw new DeviceTrustError(
          'INVALID_CHALLENGE_REQUEST', 'challengeRequest must be non-empty.');
    }

    if (challengeRequest.length > MAX_CHALLENGE_REQUEST_LENGTH) {
      throw new DeviceTrustError(
          'INVALID_CHALLENGE_REQUEST', 'challengeRequest is too large.');
    }

    if (pendingRequestCount >= MAX_CONCURRENT_REQUESTS) {
      throw new DeviceTrustError(
          'TOO_MANY_REQUESTS', 'Too many pending device attestation requests.');
    }

    pendingRequestCount++;

    const browserRequest = sendWebKitMessageWithReply(MESSAGE_HANDLER_NAME, {
      challengeRequest: challengeRequest,
    });

    let timeoutId = 0;
    const timeout = new Promise<never>((_, reject) => {
      timeoutId = window.setTimeout(() => {
        reject(new DeviceTrustError(
            'ATTESTATION_TIMEOUT',
            'Timed out waiting for device attestation response.'));
      }, REQUEST_TIMEOUT_MS);
    });

    try {
      const reply = await Promise.race([browserRequest, timeout]);
      return parseAttestationReply(reply);
    } catch (error: unknown) {
      if (error instanceof DeviceTrustError) {
        throw error;
      }
      throw new DeviceTrustError(
          'INTERNAL_ERROR', 'Unknown internal error occurred.');
    } finally {
      clearTimeout(timeoutId);
      pendingRequestCount--;
    }
  },
};

Object.freeze(DeviceTrustAPI);

function setupDeviceTrustAPI(): void {
  const globalWindow = window as any;

  try {
    if (!globalWindow.chrome) {
      Object.defineProperty(globalWindow, 'chrome', {
        value: {},
        writable: false,
        configurable: false,
        enumerable: true,
      });
    }

    if (!globalWindow.chrome.enterprise) {
      Object.defineProperty(globalWindow.chrome, 'enterprise', {
        value: {},
        writable: false,
        configurable: false,
        enumerable: true,
      });
    }

    if (globalWindow.chrome.enterprise.deviceTrust !== DeviceTrustAPI) {
      Object.defineProperty(globalWindow.chrome.enterprise, 'deviceTrust', {
        value: DeviceTrustAPI,
        writable: false,
        configurable: false,
        enumerable: true,
      });
    }
  } catch (e) {
    /* chrome.enterprise is unavailable or non-extensible */
  }
}

const deviceTrust = new CrWebApi('deviceTrust');
deviceTrust.addFunction('setupDeviceTrustAPI', setupDeviceTrustAPI);
gCrWeb.registerApi(deviceTrust);
