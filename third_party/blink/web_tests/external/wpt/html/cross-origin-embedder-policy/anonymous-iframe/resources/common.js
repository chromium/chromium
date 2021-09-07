// Create an iframe with the given 'attributes', loading a document from a given
// `origin`. The new document will execute any scripts sent toward the token it
// returns.
const newIframe = (origin, attributes = {}) => {
  const sub_document_token = token();
  let iframe = document.createElement('iframe');
  iframe.src = origin + executor_path + `&uuid=${sub_document_token}`;
  for(const i in attributes)
    iframe[i] = attributes[i];
  document.body.appendChild(iframe);
  return sub_document_token;
};

const newAnonymousIframe = origin => newIframe(origin, {anonymous: true});

const cookieFromResource = async (cookie_key, resource_token) => {
  let headers = JSON.parse(await receive(resource_token));
  return parseCookies(headers)[cookie_key];
};

// Create a navigation request in an iframe. Return the request's cookies.
const cookieFromNavigation = async function(
    cookie_key,
    iframe_origin,
    attributes = {}) {
  const resource_token = token();
  let iframe = document.createElement("iframe");
  iframe.src = `${showRequestHeaders(iframe_origin, resource_token)}`;
  for(const i in attributes)
    iframe[i] = attributes[i];
  document.body.appendChild(iframe);
  let cookie_value = await cookieFromResource(cookie_key, resource_token);
  iframe.remove();
  return cookie_value;
};

// Load a resource `type` from the iframe with `document_token`,
// return the HTTP request's cookies.
const cookieFromRequest = async function(
    cookie_key,
    document_token,
    resource_origin,
    type = "img") {
  const resource_token = token();
  send(document_token, `
    let el = document.createElement("${type}");
    el.src = "${showRequestHeaders(resource_origin, resource_token)}";
    document.body.appendChild(el);
  `);
  return await cookieFromResource(cookie_key, resource_token);
};

