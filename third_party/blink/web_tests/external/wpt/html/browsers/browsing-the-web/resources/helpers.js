window.openWindow = (url, t) => {
  const w = window.open(url);
  t?.add_cleanup(() => w.close());

  return new Promise(resolve => {
    w.addEventListener("load", () => resolve(w), { once: true });
  });
};

window.addIframe = (url = "/common/blank.html", doc = document) => {
  const iframe = doc.createElement("iframe");
  iframe.src = url;
  doc.body.append(iframe);

  return new Promise(resolve => {
    iframe.addEventListener("load", () => resolve(iframe), { once: true });
  });
};

window.waitToAvoidReplace = t => {
  return new Promise(resolve => t.step_timeout(resolve, 0));
};

window.waitForMessage = expectedSource => {
  return new Promise(resolve => {
    window.addEventListener("message", ({ source, data }) => {
      if (source === expectedSource) {
        resolve(data);
      }
    });
  });
};

window.waitForHashchange = w => {
  return new Promise(resolve => {
    w.addEventListener("hashchange", () => resolve(), { once: true });
  });
};

window.srcdocThatPostsParentOpener = text => {
  return `
    <p>${text}</p>
    <script>
      window.onload = () => {
        window.top.opener.postMessage('ready', '*');
      };
    <\/script>
  `;
};
