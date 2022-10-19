// Set the identity provider cookie.
export function set_fedcm_cookie() {
  return new Promise(resolve => {
    const img = document.createElement('img');
    img.src = 'support/set_cookie';
    img.addEventListener('error', resolve);
    document.body.appendChild(img);
  });
}

// Returns FedCM CredentialRequestOptions for which navigator.credentials.get()
// succeeds.
export function default_request_options() {
  const manifest_origin = 'https://{{hosts[][]}}:{{ports[https][0]}}';
  return {
    identity: {
      providers: [{
        configURL: manifest_origin + '/credential-management/support/fedcm/manifest.py',
        clientId: '1',
        nonce: '2',
      }]
    }
  };
}

// Test wrapper which does FedCM-specific setup.
export function fedcm_test(test_func) {
  promise_test(async t => {
    await set_fedcm_cookie();
    await test_func(t);
  });
}
