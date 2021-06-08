// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {decorate} from 'chrome://resources/js/cr/ui.m.js';
import {TabBox} from 'chrome://resources/js/cr/ui/tabs.js';

import {ClientRenderer} from './client_renderer.js';
import {initialize} from './main.js';
import {Manager} from './manager.js';

initialize(new Manager(new ClientRenderer()));
decorate('tabbox', TabBox);
