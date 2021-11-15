const PAYMENT_DETAILS = {
  total: {label: 'Total', amount: {value: '0.01', currency: 'USD'}}
};
const ICON_URL = 'https://{{hosts[][www]}}:{{ports[https][0]}}/secure-payment-confirmation/troy.png';
const AUTHENTICATOR_OPTS = {
  protocol: 'ctap2_1',
  transport: 'internal',
  hasResidentKey: true,
  hasUserVerification: true,
  isUserVerified: true,
};

// Creates and returns a WebAuthn credential, optionally with the payment
// extension set.
//
// Assumes that a virtual authenticator has already been created.
async function createCredential(set_payment_extension=true) {
  const challengeBytes = new Uint8Array(16);
  window.crypto.getRandomValues(challengeBytes);

  const publicKey = {
    challenge: challengeBytes,
    rp: {
      name: 'Acme',
    },
    user: {
      id: new Uint8Array(16),
      name: 'jane.doe@example.com',
      displayName: 'Jane Doe',
    },
    pubKeyCredParams: [{
      type: 'public-key',
      alg: -7,  // 'ES256'
    }],
    authenticatorSelection: {
      userVerification: 'required',
      residentKey: 'required',
      authenticatorAttachment: 'platform',
    },
    timeout: 30000,
  };

  if (set_payment_extension) {
    publicKey.extensions = {
      payment: { isPayment: true },
    };
  }

  return navigator.credentials.create({publicKey});
}

function arrayBufferToString(buffer) {
  return String.fromCharCode(...new Uint8Array(buffer));
}

function base64UrlEncode(data) {
  let result = btoa(data);
  return result.replace(/=+$/g, '').replace(/\+/g, "-").replace(/\//g, "_");
}

