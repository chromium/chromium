// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

alert('Prepare to DIE!');
var x = null;
return x.split('-');  // Throws exception.

function FindProxyForURL(url, host) {
  return "PROXY i-approve-this-message:42";
}
