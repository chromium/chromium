// Helper functions used in web-bundle tests.

function addElementAndWaitForLoad(element) {
  return new Promise((resolve, reject) => {
    element.onload = () => resolve(element);
    element.onerror = () => reject(element);
    document.body.appendChild(element);
  });
}

function addElementAndWaitForError(element) {
  return new Promise((resolve, reject) => {
    element.onload = () => reject(element);
    element.onerror = () => resolve(element);
    document.body.appendChild(element);
  });
}

function fetchAndWaitForReject(url) {
  return new Promise((resolve, reject) => {
    fetch(url)
      .then(() => {
        reject();
      })
      .catch(() => {
        resolve();
      });
  });
}

function isValidCrossOriginAttribute(crossorigin) {
  if (crossorigin === undefined)
    return true;
  if ((typeof crossorigin) != 'string')
    return false;
  const lower_crossorigin = crossorigin.toLowerCase();
  return (lower_crossorigin === 'anonymous') ||
         (lower_crossorigin  === 'use-credentials');
}

function addLinkAndWaitForLoad(url, resources, crossorigin) {
  return new Promise((resolve, reject) => {
    if (!isValidCrossOriginAttribute(crossorigin)) {
      reject('invalid crossorigin attribute: ' + crossorigin);
      return;
    }
    const link = document.createElement("link");
    link.rel = "webbundle";
    link.href = url;
    if (crossorigin) {
      link.crossOrigin = crossorigin;
    }
    for (const resource of resources) {
      link.resources.add(resource);
    }
    link.onload = () => resolve(link);
    link.onerror = () => reject(link);
    document.body.appendChild(link);
  });
}

function addLinkAndWaitForError(url, resources, crossorigin) {
  return new Promise((resolve, reject) => {
    if (!isValidCrossOriginAttribute(crossorigin)) {
      reject('invalid crossorigin attribute: ' + crossorigin);
      return;
    }
    const link = document.createElement("link");
    link.rel = "webbundle";
    link.href = url;
    if (crossorigin) {
      link.crossOrigin = crossorigin;
    }
    for (const resource of resources) {
      link.resources.add(resource);
    }
    link.onload = () => reject(link);
    link.onerror = () => resolve(link);
    document.body.appendChild(link);
  });
}

function addScriptAndWaitForError(url) {
  return new Promise((resolve, reject) => {
    const script = document.createElement("script");
    script.src = url;
    script.onload = reject;
    script.onerror = resolve;
    document.body.appendChild(script);
  });
}

// Currnetly Chrome supports two element types for Subresource Web Bundles
// feature, <link rel=webbundle> and <script type=webbundle>.
// In order to use the same test js file for the two types, we use
// window.TEST_WEB_BUNDLE_ELEMENT_TYPE. When 'link' is set,
// createWebBundleElement() will create a <link rel=webbundle> element, and when
// 'script' is set, createWebBundleElement() will create a <script
// rel=webbundle> element.
function isTestBundleElementTypeSet() {
  return (window.TEST_WEB_BUNDLE_ELEMENT_TYPE == 'link') ||
         (window.TEST_WEB_BUNDLE_ELEMENT_TYPE == 'script');
}

function createWebBundleElement(url, resources, options) {
  if (!isTestBundleElementTypeSet()) {
    throw new Error(
        'window.TEST_WEB_BUNDLE_ELEMENT_TYPE is not correctly set: ' +
        window.TEST_WEB_BUNDLE_ELEMENT_TYPE);
  }
  if (window.TEST_WEB_BUNDLE_ELEMENT_TYPE == 'link') {
    const link = document.createElement("link");
    link.rel = "webbundle";
    link.href = url;
    if (options) {
      if (options.crossOrigin) {
        link.crossOrigin = crossOrigin;
      }
      if (options.scopes) {
        for (const scope of options.scopes) {
          link.scopes.add(scope);
        }
      }
    }
    for (const resource of resources) {
      link.resources.add(resource);
    }
    return link;
  }
  const script = document.createElement("script");
  script.type = "webbundle";
  script.textContent =
      JSON.stringify({"source": url, "resources": resources});
  return script;
}

function addWebBundleElementAndWaitForLoad(url, resources, options) {
  const element = createWebBundleElement(url, resources, options);
  return addElementAndWaitForLoad(element);
}

function addWebBundleElementAndWaitForError(url, resources, options) {
  const element = createWebBundleElement(url, resources, options);
  return addElementAndWaitForError(element);
}

