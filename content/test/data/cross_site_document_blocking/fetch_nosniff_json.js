// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

fetch('/site_isolation/nosniff.json')
    .then(response => response.text())
    .then(text => domAutomationController.send('BODY: ' + text))
    .catch(err => domAutomationController.send('ERROR: ' + err));
