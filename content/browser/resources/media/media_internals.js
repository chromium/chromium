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
