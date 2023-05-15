// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function createAttributionSrcImg(src) {
  const img = document.createElement('img');
  img.attributionSrc = src;
}

function createAttributionEligibleImgSrc(src) {
  const img = document.createElement('img');
  img.setAttribute('attributionsrc', '');
  img.src = src;
}

function createAttributionSrcScript(src) {
  const script = document.createElement('script');
  script.setAttribute('attributionsrc', src);
}

function createAttributionEligibleScriptSrc(src) {
  const script = document.createElement('script');
  script.setAttribute('attributionsrc', '');
  script.src = src;
  document.body.appendChild(script);
}

function doAttributionEligibleFetch(url) {
  // Optionally set keepalive to ensure the request outlives the page.
  fetch(url, {
    attributionReporting: {eventSourceEligible: true, triggerEligible: false},
    keepalive: true
  });
}

function doAttributionEligibleXHR(url) {
  const req = new XMLHttpRequest();
  req.open('GET', url);
  req.setAttributionReporting(
      {eventSourceEligible: true, triggerEligible: false});
  req.send();
}

function createAttributionSrcAnchor({
  id,
  url,
  attributionsrc,
  target = '_top',
  left,
  top,
} = {}) {
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.target = target;
  anchor.attributionSrc = attributionsrc;
  anchor.width = 100;
  anchor.height = 100;
  anchor.id = id;

  if (left !== undefined && top !== undefined) {
    const style = 'position: absolute; left: ' + (left - 10) +
        'px; top: ' + (top - 10) + 'px; width: 20px; height: 20px;';
    anchor.setAttribute('style', style);
  }

  anchor.innerText = 'This is link';

  document.body.appendChild(anchor);
  return anchor;
}

function createAndClickAttributionSrcAnchor(params) {
  const anchor = createAttributionSrcAnchor(params);
  simulateClick(anchor);
  return anchor;
}
