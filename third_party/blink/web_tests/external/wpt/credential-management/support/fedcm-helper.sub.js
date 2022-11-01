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
export function default_request_options(manifest_filename) {
  if (manifest_filename === undefined) {
    manifest_filename = "manifest.py";
  }
  const manifest_path = `https://{{host}}:{{ports[https][0]}}/\
credential-management/support/fedcm/${manifest_filename}`;
  return {
    identity: {
      providers: [{
        configURL: manifest_path,
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

function select_manifest_impl(manifest_url) {
  const url_query = (manifest_url === undefined)
      ? '' : '?manifest_url=${manifest_url}';

  return new Promise(resolve => {
    const img = document.createElement('img');
    img.src = 'support/fedcm/select_manifest_in_root_manifest.py?${url_query}';
    img.addEventListener('error', resolve);
    document.body.appendChild(img);
  });
}

// Sets the manifest returned by the next fetch of /.well-known/web_identity
// select_manifest() only affects the next fetch and not any subsequent fetches
// (ex second next fetch).
export function select_manifest(test, test_options) {
  // Add cleanup in case that /.well-known/web_identity is not fetched at all.
  test.add_cleanup(async () => {
    await select_manifest_impl();
  });
  const manifest_url = test_options.identity.providers[0].configURL;
  return select_manifest_impl(manifest_url);
}
