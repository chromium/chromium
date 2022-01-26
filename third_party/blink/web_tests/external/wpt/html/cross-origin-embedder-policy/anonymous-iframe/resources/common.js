// Create an anonymous iframe. The new document will execute any scripts sent
// toward the token it returns.
const newAnonymousIframe = (child_origin) => {
  const sub_document_token = token();
  let iframe = document.createElement('iframe');
  iframe.src = child_origin + executor_path + `&uuid=${sub_document_token}`;
  iframe.anonymous = true;
  document.body.appendChild(iframe);
  return sub_document_token;
};

// Create a normal iframe. The new document will execute any scripts sent
// toward the token it returns.
const newIframe = (child_origin) => {
  const sub_document_token = token();
  let iframe = document.createElement('iframe');
  iframe.src = child_origin + executor_path + `&uuid=${sub_document_token}`;
  iframe.anonymous = false
  document.body.appendChild(iframe);
  return sub_document_token;
};

// Create a popup. The new document will execute any scripts sent toward the
// token it returns.
const newPopup = (origin, opt_headers) => {
  opt_headers ||= "";
  const popup_token = token();
  const popup = window.open(origin + executor_path + opt_headers +
    `&uuid=${popup_token}`);
  add_completion_callback(() => popup.close());
  return popup_token;
}

// Create a fenced frame. The new document will execute any scripts sent
// toward the token it returns.
const newFencedFrame = (child_origin, opt_headers) => {
  opt_headers ||= "";
  const support_loading_mode_fenced_frame =
    "|header(Supports-Loading-Mode,fenced-frame)";
  const sub_document_token = token();
  const fencedframe = document.createElement('fencedframe');
  fencedframe.src = child_origin + executor_path +
    support_loading_mode_fenced_frame + opt_headers +
    `&uuid=${sub_document_token}`;
  document.body.appendChild(fencedframe);
  return sub_document_token;
};

const importScript = (url) => {
  const script = document.createElement("script");
  script.type = "text/javascript";
  script.src = url;
  const loaded = new Promise(resolve => script.onload = resolve);
  document.body.appendChild(script);
  return loaded;
}
