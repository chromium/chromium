// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var media = {};

// <include src="main.js">
// <include src="util.js">
// <include src="player_info.js">
// <include src="manager.js">
// <include src="manager_experiment_disabler.js">
// <include src="client_renderer.js">

media.initialize(new Manager(new ClientRenderer()));
if (cr.ui) {
  cr.ui.decorate('tabbox', cr.ui.TabBox);
}
