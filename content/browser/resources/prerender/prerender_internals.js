// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';
import {PrerenderInternalsHandler} from './prerender_internals.mojom-webui.js';

let pageHandler;

function addDivElement(li, text) {
  const div = document.createElement('div');
  div.appendChild(document.createTextNode(text));
  li.appendChild(div);
}

document.addEventListener('DOMContentLoaded', _ => {
  pageHandler = PrerenderInternalsHandler.getRemote();

  pageHandler.getPrerenderInfo().then((response) => {
    for (const info of response.infos) {
      const li = document.createElement('li');
      for (const pageInfo of info.prerenderedPageInfos) {
        addDivElement(li, pageInfo.triggerPageUrl.url);
        addDivElement(li, pageInfo.url.url);
        addDivElement(li, pageInfo.finalStatus);
      }
      $('prerender-info').appendChild(li);
    }
  });
});
