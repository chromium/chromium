// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import {ClientRenderer} from './client_renderer.js';
import {initialize} from './main.js';
import {Manager} from './manager.js';

initialize(new Manager(new ClientRenderer()));
const tabBox = document.querySelector('cr-tab-box');
tabBox.hidden = false;

const _TabIndicies = {
  '#players': 0,
  '#audio': 1,
  '#video-capture': 2,
  '#audio-focus': 3,
  '#cdms': 4,
};

const tabHash = window.location.hash.toLowerCase();
if (tabHash in _TabIndicies) {
  tabBox.setAttribute('selected-index', _TabIndicies[tabHash]);
}
