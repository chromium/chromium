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
  const manifest_origin = 'https://{{host}}:{{ports[https][0]}}';
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
