// Common mock values for the mockAuthenticator.
export const CHALLENGE = new TextEncoder().encode("climb a mountain");

export const PUBLIC_KEY_RP = {
    id: "subdomain.example.test",
    name: "Acme"
};

export const PUBLIC_KEY_USER = {
    id: new TextEncoder().encode("1098237235409872"),
    name: "avery.a.jones@example.com",
    displayName: "Avery A. Jones",
    icon: "https://pics.acme.com/00/p/aBjjjpqPb.png"
};

export const PUBLIC_KEY_PARAMETERS = [{
    type: "public-key",
    alg: -7,
},];

export const AUTHENTICATOR_SELECTION_CRITERIA = {
    requireResidentKey: false,
    userVerification: "preferred",
};

export const MAKE_CREDENTIAL_OPTIONS = {
    challenge: CHALLENGE,
    rp: PUBLIC_KEY_RP,
    user: PUBLIC_KEY_USER,
    pubKeyCredParams: PUBLIC_KEY_PARAMETERS,
    authenticatorSelection: AUTHENTICATOR_SELECTION_CRITERIA,
    excludeCredentials: [],
};

export const ACCEPTABLE_CREDENTIAL_ID =
    new TextEncoder().encode("acceptableCredential");

export const ACCEPTABLE_CREDENTIAL = {
    type: "public-key",
    id: ACCEPTABLE_CREDENTIAL_ID,
    transports: ["usb", "nfc", "ble"]
};

export const GET_CREDENTIAL_OPTIONS = {
    challenge: CHALLENGE,
    rpId: "subdomain.example.test",
    allowCredentials: [ACCEPTABLE_CREDENTIAL],
    userVerification: "preferred",
};

export const RAW_ID = new TextEncoder("utf-8").encode("rawId");
export const ID = btoa("rawId");
export const CLIENT_DATA_JSON =
    new TextEncoder("utf-8").encode("clientDataJSON");
export const ATTESTATION_OBJECT =
    new TextEncoder("utf-8").encode("attestationObject");
export const AUTHENTICATOR_DATA =
    new TextEncoder("utf-8").encode("authenticatorData");
export const SIGNATURE = new TextEncoder("utf-8").encode("signature");

export const CABLE_AUTHENTICATION = {
    version: 1,
    clientEid: new TextEncoder("utf-8").encode("SixteenByteClEid"),
    authenticatorEid: new TextEncoder("utf-8").encode("SixteenByteAuEid"),
    sessionPreKey: new TextEncoder("utf-8").encode('x'.repeat(32)),
};

function wrapScriptWithImportedTestInput(inputName, script) {
  return "(async function() {" +
      `  const {${inputName}} = ` +
      "      await import('/credentialmanagement/resources/test-inputs.js');" +
      "  return " + script + "})();";
}

export const CREATE_CREDENTIALS = wrapScriptWithImportedTestInput(
    "MAKE_CREDENTIAL_OPTIONS",
    "navigator.credentials.create({publicKey : MAKE_CREDENTIAL_OPTIONS})" +
        ".then(c => window.parent.postMessage(String(c), \'*\'))" +
        ".catch(e => window.parent.postMessage(String(e), \'*\'));");


export const GET_CREDENTIAL = wrapScriptWithImportedTestInput(
    "GET_CREDENTIAL_OPTIONS",
    "navigator.credentials.get({publicKey : GET_CREDENTIAL_OPTIONS})" +
        ".then(c => window.parent.postMessage(String(c), \'*\'))" +
        ".catch(e => window.parent.postMessage(String(e), \'*\'));");

export function encloseInScriptTag(code) {
  return "<script>" + code + "</scr" + "ipt>";
}

export function deepCopy(value) {
  if ([Number, String, Boolean, Uint8Array].includes(value.constructor))
    return value;

  let copy = (value.constructor == Array) ? [] : {};
  for (let key of Object.keys(value))
    copy[key] = deepCopy(value[key]);
  return copy;
}

// Verifies if |r| is the valid response to credentials.create(publicKey).
export function assertValidMakeCredentialResponse(r) {
    assert_equals(r.id, ID, 'id');
    assert_true(r.rawId instanceof ArrayBuffer);
    assert_array_equals(new Uint8Array(r.rawId),
        RAW_ID, "rawId returned is the same");
    assert_true(r.response instanceof AuthenticatorAttestationResponse);
    assert_true(r.response.clientDataJSON instanceof ArrayBuffer);
    assert_array_equals(new Uint8Array(r.response.clientDataJSON),
        CLIENT_DATA_JSON, "clientDataJSON returned is the same");
    assert_true(r.response.attestationObject instanceof ArrayBuffer);
    assert_array_equals(new Uint8Array(r.response.attestationObject),
        ATTESTATION_OBJECT, "attestationObject returned is the same");
    assert_false('authenticatorData' in r.response);
    assert_false('signature' in r.response);
}

// Verifies if |r| is the valid response to credentials.get(publicKey).
export function assertValidGetCredentialResponse(r) {
    assert_equals(r.id, ID, 'id');
    assert_true(r.rawId instanceof ArrayBuffer);
    assert_array_equals(new Uint8Array(r.rawId),
        RAW_ID, "rawId returned is the same");

    // The call returned an AssertionResponse, meaning it has
    //  authenticatorData and signature and does not have an attestationObject.
    assert_true(r.response instanceof AuthenticatorAssertionResponse);
    assert_true(r.response.clientDataJSON instanceof ArrayBuffer);
    assert_array_equals(new Uint8Array(r.response.clientDataJSON),
        CLIENT_DATA_JSON, "clientDataJSON returned is the same");
    assert_true(r.response.authenticatorData instanceof ArrayBuffer);
    assert_true(r.response.signature instanceof ArrayBuffer);
    assert_array_equals(new Uint8Array(r.response.authenticatorData),
        AUTHENTICATOR_DATA, "authenticator_data returned is the same");
    assert_array_equals(new Uint8Array(r.response.signature),
        SIGNATURE, "signature returned is the same");
    assert_true(r.response.userHandle == null);
    assert_false('attestationObject' in r.response);
}

// Sets up a virtual authenticator with |authenticatorArgs| for the duration of
// the tests run in |t|.
export function authenticatorSetup(manager, name, t, authenticatorArgs) {
  promise_test(
      () => manager.createAuthenticator(authenticatorArgs),
      name + ': Setup up the testing environment.');
  try {
    t();
  } finally {
    promise_test(
        () => manager.clearAuthenticators(),
        name + ': Clean up testing environment.');
  }
}
