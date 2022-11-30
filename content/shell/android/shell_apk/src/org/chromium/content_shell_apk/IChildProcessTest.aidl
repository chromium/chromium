// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import android.os.Bundle;

/**
  * Interface provided to the TestChildProcessService. Used to echo back the calls made on the
  * ChildProcessServiceDelegate to the test process.
  */
interface IChildProcessTest {
  // Called by the service when onConnectionSetup is received. Echos back the parameters received
  // so far.
  oneway void onConnectionSetup(boolean serviceCreatedCalled, in Bundle serviceBundle, in Bundle connectionBundle);

  oneway void onLoadNativeLibrary(boolean loadedSuccessfully);

  oneway void onBeforeMain(in String[] commandLine);

  oneway void onRunMain();
}
