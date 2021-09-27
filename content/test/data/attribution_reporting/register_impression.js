// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function simulateClick(target) {
  simulateClickWithButton(target, 0 /* left click */);
}

function simulateMiddleClick(target) {
  simulateClickWithButton(target, 1 /* middle click */);
}

function simulateClickWithButton(target, button) {
  target = document.getElementById(target);
  let evt = new MouseEvent('click', {'button': button});
  return target.dispatchEvent(evt);
}

function createImpressionTag({
  id,
  url,
  data,
  destination,
  target = '_top',
  reportOrigin,
  expiry,
  priority,
  registerAttributionSource = false,
  left,
  top,
} = {}) {
  let anchor = document.createElement('a');
  anchor.href = url;
  anchor.setAttribute('attributionsourceeventid', data);
  anchor.setAttribute('attributiondestination', destination);
  anchor.setAttribute('target', target);
  anchor.width = 100;
  anchor.height = 100;
  anchor.id = id;

  if (reportOrigin !== undefined)
    anchor.setAttribute('attributionreportto', reportOrigin);

  if (expiry !== undefined)
    anchor.setAttribute('attributionexpiry', expiry);

  if (priority !== undefined)
    anchor.setAttribute('attributionsourcepriority', priority);

  if (registerAttributionSource)
    anchor.setAttribute('registerattributionsource', '');

  if (left !== undefined && top !== undefined) {
    const style = 'position: absolute; left: ' + (left - 10) +
        'px; top: ' + (top - 10) + 'px; width: 20px; height: 20px;';
    anchor.setAttribute('style', style);
  }

  // Create the text node for anchor element.
  let link = document.createTextNode('This is link');

  // Append the text node to anchor element.
  anchor.appendChild(link);
  document.body.appendChild(anchor);

  return anchor;
}
