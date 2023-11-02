// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapk_install;

import org.chromium.components.webapk_install.IOnFinishInstallCallback;

oneway interface IWebApkInstallCoordinatorService {

    /**
     * Schedule a WebAPK installation in the WebApkInstallCoordinatorService in Chrome.
     * Chrome will handle the installation and invoke the {@code callback} when the
     * installation succeeded or failed.
     */
    void scheduleInstallAsync(
      in byte[] apkProto,
      in Bitmap primaryIcon,
      boolean isPrimaryIconMaskable,
      IOnFinishInstallCallback callback
    );
}
