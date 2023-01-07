// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.require('moduleB.TestInterfaceRemote');

// This is not expected to do anything useful, but it must compile.
const remote = moduleB.TestInterface.getRemote();
remote.passA1({'q': '', 'r': '', 's': ''});
