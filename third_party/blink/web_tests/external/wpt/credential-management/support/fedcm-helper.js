// Set the identity provider cookie.
export function set_fedcm_cookie() {
  return new Promise(resolve => {
    const img = document.createElement('img');
    img.src = 'support/set_cookie';
    img.addEventListener('error', resolve);
    document.body.appendChild(img);
  });
}
