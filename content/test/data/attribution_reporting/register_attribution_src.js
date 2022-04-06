// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function createAttributionSrcImg(src) {
  const img = document.createElement('img');
  img.setAttribute('target', "top");
  img.width = 100;
  img.height = 100;
  img.setAttribute("attributionsrc", src);
  document.body.appendChild(img);
  return img;
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
  anchor.setAttribute('target', target);
  anchor.setAttribute("attributionsrc", attributionsrc);
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
