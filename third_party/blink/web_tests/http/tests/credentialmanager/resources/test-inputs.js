'use strict';

// A minimal HTML document that loads this very file. This can be
// document.write()'en to nested documents to include test input/outputs.
const HTML_WITH_TEST_INPUTS = "<script src='/credentialmanager/resources/test-inputs.js'></script>";

// Constructs a script that loads this very script file, and runs |code| once
// loaded. This can be window.eval()'ed in nested documents to include test
// input/outputs (and then run the |code| under test).
function getScriptThatLoadsTestInputsAndRuns(code) {
  return "var script = document.createElement('script');"
       + "script.src = '/credentialmanager/resources/test-inputs.js';"
       + "script.onload = _ => {" + code + "};"
       + "document.head.appendChild(script);"
}

// Common mock values for the mockAuthenticator.
var CHALLENGE = new TextEncoder().encode("climb a mountain");

var PUBLIC_KEY_RP = {
    id: "subdomain.example.test",
    name: "Acme"
};

var PUBLIC_KEY_USER = {
    id: new TextEncoder().encode("1098237235409872"),
    name: "avery.a.jones@example.com",
    displayName: "Avery A. Jones",
    icon: "https://pics.acme.com/00/p/aBjjjpqPb.png"
};

var PUBLIC_KEY_PARAMETERS =  [{
    type: "public-key",
    alg: -7,
},];

var AUTHENTICATOR_SELECTION_CRITERIA = {
    requireResidentKey: false,
    userVerification: "preferred",
};

var MAKE_CREDENTIAL_OPTIONS = {
    challenge: CHALLENGE,
    rp: PUBLIC_KEY_RP,
    user: PUBLIC_KEY_USER,
    pubKeyCredParams: PUBLIC_KEY_PARAMETERS,
    authenticatorSelection: AUTHENTICATOR_SELECTION_CRITERIA,
    excludeCredentials: [],
};

var ACCEPTABLE_CREDENTIAL_ID = new TextEncoder().encode("acceptableCredential");

var ACCEPTABLE_CREDENTIAL = {
    type: "public-key",
    id: ACCEPTABLE_CREDENTIAL_ID,
    transports: ["usb", "nfc", "ble"]
};

var GET_CREDENTIAL_OPTIONS = {
    challenge: CHALLENGE,
    rpId: "subdomain.example.test",
    allowCredentials: [ACCEPTABLE_CREDENTIAL],
    userVerification: "preferred",
};

var RAW_ID = new TextEncoder("utf-8").encode("rawId");
var ID = btoa("rawId");
var CLIENT_DATA_JSON = new TextEncoder("utf-8").encode("clientDataJSON");
var ATTESTATION_OBJECT = new TextEncoder("utf-8").encode("attestationObject");
var AUTHENTICATOR_DATA = new TextEncoder("utf-8").encode("authenticatorData");
var SIGNATURE = new TextEncoder("utf-8").encode("signature");
var CABLE_REGISTRATION  = {
    versions: [1],
    rpPublicKey: new TextEncoder("utf-8").encode("SixteenByteRpKey"),
};

var CABLE_AUTHENTICATION = {
    version: 1,
    clientEid: new TextEncoder("utf-8").encode("SixteenByteClEid"),
    authenticatorEid: new TextEncoder("utf-8").encode("SixteenByteAuEid"),
    sessionPreKey: new TextEncoder("utf-8").encode('x'.repeat(32)),
};

var CREATE_CREDENTIALS =
    "navigator.credentials.create({publicKey : MAKE_CREDENTIAL_OPTIONS})"
    + ".then(c => window.parent.postMessage(String(c), '*'))";
    + ".catch(e => window.parent.postMessage(String(e), '*'));";

var GET_CREDENTIAL = "navigator.credentials.get({publicKey : GET_CREDENTIAL_OPTIONS})"
    + ".then(c => window.parent.postMessage(String(c), '*'))";
    + ".catch(e => window.parent.postMessage(String(e), '*'));";

function encloseInScriptTag(code) {
  return "<script>" + code + "</scr" + "ipt>";
}

function deepCopy(value) {
  if ([Number, String, Boolean, Uint8Array].includes(value.constructor))
    return value;

  let copy = (value.constructor == Array) ? [] : {};
  for (let key of Object.keys(value))
    copy[key] = deepCopy(value[key]);
  return copy;
}

// Verifies if |r| is the valid response to credentials.create(publicKey).
function assertValidMakeCredentialResponse(r) {
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
function assertValidGetCredentialResponse(r) {
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
