// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.app;

import android.app.Service;
import android.content.Context;

import org.chromium.base.process_launcher.ChildProcessService;
import org.chromium.content.app.ContentChildProcessServiceDelegate;

/** Factory to create a service class that can call through to the content implementation. */
public class ChildProcessServiceFactory {
    public static ChildProcessService create(Service service, Context context) {
        return new ChildProcessService(new ContentChildProcessServiceDelegate(), service, context);
    }

    private ChildProcessServiceFactory() {}
}
